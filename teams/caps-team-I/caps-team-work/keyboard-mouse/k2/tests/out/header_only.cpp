#include <Platform/KbmEngineKeys.hpp>
int main() { return RC::Platform::Kbm::key_table().size() == 163 ? 0 : 1; }
