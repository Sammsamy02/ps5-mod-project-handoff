#include "../PATCH/PakRequestProducer.hpp"
#include <iostream>
int main(int argc,char**argv){try{if(argc!=2)return 2;auto rows=ModBridge::PakMount::discover_requests(argv[1]);for(const auto&r:rows)std::cout<<r.pak_path_abs<<'\n';return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
