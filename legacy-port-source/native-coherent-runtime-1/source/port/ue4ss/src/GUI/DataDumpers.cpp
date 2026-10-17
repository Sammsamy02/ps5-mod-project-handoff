#include <cstdint>
#include <ctime>
#include <limits>
#include <stdexcept>
#include <string>
#include <Constructs/Views/EnumerateView.hpp>
#include <DynamicOutput/Output.hpp>
#include <File/File.hpp>
#ifdef RC_FILE_POSIX_SELECTED
#include <Helpers/Utf16Utf8.hpp>
#endif
#include <GUI/Dumpers.hpp>
#include <UE4SSProgram.hpp>
#include <Unreal/AActor.hpp>
#include <Unreal/Searcher/ObjectSearcher.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>

namespace RC::GUI::Dumpers
{
    using namespace ::RC::Unreal;

    auto generate_root_component_csv(UObject* root_component) -> StringType
    {
        StringType root_actor_buffer{};

        static auto location_property = root_component->GetPropertyByNameInChain(FromCharTypePtr<TCHAR>(STR("RelativeLocation")));
        static auto rotation_property = root_component->GetPropertyByNameInChain(FromCharTypePtr<TCHAR>(STR("RelativeRotation")));
        static auto scale_property = root_component->GetPropertyByNameInChain(FromCharTypePtr<TCHAR>(STR("RelativeScale3D")));

        auto location = root_component->GetValuePtrByPropertyNameInChain<FVector>(FromCharTypePtr<TCHAR>(STR("RelativeLocation")));
        FString location_string{};
        location_property->ExportTextItem(location_string, location, nullptr, nullptr, 0);
        root_actor_buffer.append(fmt::format(STR("\"{}\","), *location_string));

        auto rotation = root_component->GetValuePtrByPropertyNameInChain<FRotator>(FromCharTypePtr<TCHAR>(STR("RelativeRotation")));
        FString rotation_string{};
        rotation_property->ExportTextItem(rotation_string, rotation, nullptr, nullptr, 0);
        root_actor_buffer.append(fmt::format(STR("\"{}\","), *rotation_string));

        auto scale = root_component->GetValuePtrByPropertyNameInChain<FVector>(FromCharTypePtr<TCHAR>(STR("RelativeScale3D")));
        FString scale_string{};
        scale_property->ExportTextItem(scale_string, scale, nullptr, nullptr, 0);
        root_actor_buffer.append(fmt::format(STR("\"{}\","), *scale_string));

        return root_actor_buffer;
    }

    static auto generate_actors_csv_file(UClass* dump_actor_class) -> StringType
    {
        StringType file_buffer{};

        file_buffer.append(STR("---,Actor,Location,Rotation,Scale,Meshes\n"));

        size_t actor_count{};
        FindObjectSearcher(dump_actor_class, AnySuperStruct::StaticClass()).ForEach([&](UObject* object) {
            if (object->HasAnyFlags(RF_ClassDefaultObject))
            {
                return LoopAction::Continue;
            }

            auto actor = static_cast<AActor*>(object);

            auto root_component = actor->GetValuePtrByPropertyNameInChain<UObject*>(FromCharTypePtr<TCHAR>(STR("RootComponent")));
            if (!root_component || !*root_component)
            {
                return LoopAction::Continue;
            }

            StringType actor_buffer{};

            actor_buffer.append(fmt::format(STR("Row_{},"), actor_count));

            static auto game_mode_base = UObjectGlobals::FindFirstOf(STR("GameModeBase"));
            static auto class_property = game_mode_base->GetPropertyByNameInChain(FromCharTypePtr<TCHAR>(STR("GameStateClass")));
            FString actor_class_string{};
            class_property->ExportTextItem(actor_class_string, &actor->GetClassPrivate(), nullptr, nullptr, 0);
            actor_buffer.append(fmt::format(STR("{},"), *actor_class_string));

            // TODO: build system to handle other types of components - possibly including a way to specify which components to dump and which properties are important via a config file
            actor_buffer.append(generate_root_component_csv(*root_component));
            actor_buffer.append(STR("\""));
            static auto static_mesh_component_class = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/Engine.StaticMeshComponent"));
            const auto& static_mesh_components = actor->K2_GetComponentsByClass(static_mesh_component_class);
            if (static_mesh_components.Num() > 0)
            {
                actor_buffer.append(STR("("));
                for (auto [static_mesh_component_ptr, static_mesh_component_index] : static_mesh_components | views::enumerate)
                {
                    const auto mesh = *static_mesh_component_ptr->GetValuePtrByPropertyNameInChain<UObject*>(FromCharTypePtr<TCHAR>(STR("StaticMesh")));
                    if (!mesh)
                    {
                        Output::send<LogLevel::Warning>(STR("SKIPPING COMPONENT! StaticMeshComponent '{}' has no mesh.\n"),
                                                        static_mesh_component_ptr->GetOuterPrivate()->GetName());
                        continue;
                    }

                    static auto mesh_property = static_mesh_component_ptr->GetPropertyByNameInChain(FromCharTypePtr<TCHAR>(STR("StaticMesh")));
                    FString mesh_string{};
                    mesh_property->ExportTextItem(mesh_string, &mesh, nullptr, nullptr, 0);
                    actor_buffer.append(fmt::format(STR("(StaticMesh={}',"), *mesh_string));

                    auto materials_for_each_body = [&](const UObject* material_interface) {
                        if (material_interface)
                        {
                            auto material_full_name = material_interface->GetOuterPrivate()->GetFullName();
                            const auto material_type_space_location = material_full_name.find(STR(" "));
                            if (material_type_space_location == material_full_name.npos)
                            {
                                Output::send<LogLevel::Warning>(STR("SKIPPING MATERIAL! Was unable to find space in full material name in component: '{}'.\n"),
                                                                material_full_name);
                                return;
                            }

                            if (material_type_space_location > static_cast<unsigned long long>(std::numeric_limits<long long>::max()))
                            {
                                throw std::runtime_error{"integer overflow when converting material_type_space_location signed\n"};
                            }
                            auto material_typeless_name = StringViewType{material_full_name.begin() + static_cast<long long>(material_type_space_location) + 1,
                                                                         material_full_name.end()};

                            actor_buffer.append(fmt::format(STR("{}'"), material_interface->GetClassPrivate()->GetName()));
                            actor_buffer.append(fmt::format(STR("\"\"{}"), material_typeless_name));
                            actor_buffer.append(STR("\"\"'"));
                        }
                    };

                    TArray<UObject*> material_interfaces;
                    if (auto static_materials_property = CastField<FArrayProperty>(mesh->GetPropertyByName((STR("StaticMaterials")))))
                    {
                        auto static_materials_inner_property = static_materials_property->GetInner();

                        if (auto static_material_property = CastField<FStructProperty>(static_materials_inner_property))
                        {
                            auto& static_material_struct = static_material_property->GetStruct();
                            auto material_interface_property = static_material_struct->GetPropertyByNameInChain((STR("MaterialInterface")));

                            FScriptArrayHelper_InContainer array_helper(static_materials_property, mesh);
                            for (int32 i = 0; i < array_helper.Num(); i++)
                            {
                                auto material = *material_interface_property->ContainerPtrToValuePtr<UObject*>(array_helper.GetRawPtr(i));
                                material_interfaces.Add(material);
                            }
                        }
                    }

                    if (material_interfaces.GetData())
                    {
                        actor_buffer.append(STR("Materials=("));
                    }
                    for (auto [material, material_index] : material_interfaces | views::enumerate)
                    {
                        materials_for_each_body(material);
                        if (material_index + 1 < material_interfaces.Num())
                        {
                            actor_buffer.append(STR(","));
                        }
                        else
                        {
                            actor_buffer.append(STR(")"));
                        }
                    }

                    actor_buffer.append(STR(")"));

                    if (static_mesh_component_index + 1 < static_mesh_components.Num())
                    {
                        actor_buffer.append(STR(","));
                    }
                }
                actor_buffer.append(STR(")"));
            }
            actor_buffer.append(STR("\"\n"));
            file_buffer.append(actor_buffer);

            ++actor_count;
            return LoopAction::Continue;
        });

        return file_buffer;
    }

    auto call_generate_static_mesh_file() -> void
    {
        Output::send(STR("Dumping CSV of all loaded static mesh actors, positions and mesh properties\n"));
        static auto dump_actor_class = UObjectGlobals::StaticFindObject<UClass*>(nullptr, nullptr, STR("/Script/Engine.StaticMeshActor"));
        StringType file_buffer{};
        file_buffer.append(generate_actors_csv_file(dump_actor_class));
        auto file =
                File::open(
#ifdef RC_FILE_POSIX_SELECTED
                           std::filesystem::path{Unicode::EncodeUtf8(UE4SSProgram::get_program().get_working_directory())} /
                                   fmt::format("{}-ue4ss_static_mesh_data.csv", long(std::time(nullptr))),
#else
                           fmt::format(STR("{}\\{}-ue4ss_static_mesh_data.csv"), UE4SSProgram::get_program().get_working_directory(), long(std::time(nullptr))),
#endif
                           File::OpenFor::Writing,
                           File::OverwriteExistingFile::Yes,
                           File::CreateIfNonExistent::Yes);
        file.write_string_to_file(file_buffer);
        Output::send(STR("Finished dumping CSV of all loaded static mesh actors, positions and mesh properties\n"));
    }

    auto call_generate_all_actor_file() -> void
    {
        Output::send(STR("Dumping CSV of all loaded actor types, positions and mesh properties\n"));
        StringType file_buffer{};
        file_buffer.append(generate_actors_csv_file(AActor::StaticClass()));
        auto file = File::open(
#ifdef RC_FILE_POSIX_SELECTED
                           std::filesystem::path{Unicode::EncodeUtf8(UE4SSProgram::get_program().get_working_directory())} /
                                   fmt::format("{}-ue4ss_actor_data.csv", long(std::time(nullptr))),
#else
                           fmt::format(STR("{}\\{}-ue4ss_actor_data.csv"), UE4SSProgram::get_program().get_working_directory(), long(std::time(nullptr))),
#endif
                               File::OpenFor::Writing,
                               File::OverwriteExistingFile::Yes,
                               File::CreateIfNonExistent::Yes);
        file.write_string_to_file(file_buffer);
        Output::send(STR("Finished dumping CSV of all loaded actor types, positions and mesh properties\n"));
    }

} // namespace RC::GUI::Dumpers
