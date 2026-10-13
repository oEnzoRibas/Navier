#pragma once
#include <navier/navier.hpp>
#include <navier/format/binary.hpp>
#include <charconv>
#include <navier/query/query_engine.hpp>
#include <iomanip>
#include <iostream>
#include <limits>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace navier::tools {
inline std::uint64_t unsigned_number(std::string_view text){std::uint64_t value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);if(error!=std::errc{}||end!=text.data()+text.size())throw std::invalid_argument("Expected uint64 integer");return value;}
inline void json_string(std::ostream& out,std::string_view value){out<<'"';for(unsigned char c:value){switch(c){case '"':out<<"\\\"";break;case '\\':out<<"\\\\";break;case '\n':out<<"\\n";break;case '\r':out<<"\\r";break;case '\t':out<<"\\t";break;default:if(c<32){constexpr char hex[]="0123456789abcdef";out<<"\\u00"<<hex[c>>4]<<hex[c&15];}else out<<static_cast<char>(c);}}out<<'"';}
inline void field_json(std::ostream& out,const storage::FieldDescriptor& field){out<<"{\"id\":";json_string(out,field.name);out<<",\"scalarType\":\""<<(field.scalar_type==storage::ScalarType::float32?"float32":"float64")<<"\",\"dimensions\":[";for(std::size_t i=0;i<field.dimensions.size();++i){if(i)out<<',';out<<field.dimensions[i];}out<<"],\"unit\":";json_string(out,field.unit);out<<'}';}
inline void checkpoint_json(std::ostream& out,const storage::CheckpointDescriptor& cp){out<<"{\"step\":\""<<cp.step<<"\",\"time\":"<<cp.time<<",\"generation\":\""<<cp.generation<<"\",\"kind\":\""<<(cp.kind==storage::CheckpointKind::analysis?"analysis":"restart")<<"\",\"fields\":[";for(std::size_t i=0;i<cp.fields.size();++i){if(i)out<<',';field_json(out,cp.fields[i]);}out<<"],\"metrics\":[";for(std::size_t i=0;i<cp.metrics.size();++i){if(i)out<<',';out<<"{\"name\":";json_string(out,cp.metrics[i].name);out<<",\"value\":"<<cp.metrics[i].value<<",\"unit\":";json_string(out,cp.metrics[i].unit);out<<'}';}out<<"]}";}
inline void raw_field(std::ostream& out,const storage::Field& field){std::array<std::byte,65536> buffer{};std::visit([&](const auto& values){using T=typename std::decay_t<decltype(values)>::value_type;using Bits=std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>;for(std::size_t base=0;base<values.size();){const auto count=std::min(values.size()-base,buffer.size()/sizeof(T));for(std::size_t i=0;i<count;++i)format::put(buffer,i*sizeof(T),std::bit_cast<Bits>(values[base+i]),sizeof(T));out.write(reinterpret_cast<const char*>(buffer.data()),static_cast<std::streamsize>(count*sizeof(T)));if(!out)throw std::runtime_error("Field export failed");base+=count;}},field.values);}
inline bool is_v2(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);std::array<char,8> magic{};in.read(magic.data(),8);return in&&std::string_view(magic.data(),8)=="NAVIER02";}
inline int dataset_cli(int argc,char** argv){
    const std::string_view command=argv[1];const std::filesystem::path path(argv[2]);storage::DatasetStore store(path);std::cout<<std::setprecision(17);
    if((command=="info"||command=="info-json")&&argc==3){const auto stats=store.statistics();const auto [first,last]=store.step_bounds();if(command=="info-json"){std::cout<<"{\"format\":\"NAVIER02\",\"count\":"<<store.size()<<",\"bytes\":"<<std::filesystem::file_size(path)<<",\"firstStep\":\""<<first<<"\",\"lastStep\":\""<<last<<"\",\"indexPages\":"<<stats.index_pages<<",\"pageSize\":"<<storage::page_size<<",\"rootPage\":\""<<stats.root_page<<"\",\"recordsScanned\":"<<stats.records_scanned<<",\"bytesRead\":"<<stats.bytes_read<<",\"recoveredTail\":"<<(stats.recovered_tail?"true":"false")<<",\"metadataJson\":";json_string(std::cout,store.metadata());std::cout<<'}';}else std::cout<<"NAVIER02\nFile bytes: "<<std::filesystem::file_size(path)<<"\nCheckpoints: "<<store.size()<<"\nSteps: "<<first<<".."<<last<<"\nIndex: persistent B+ Tree, "<<stats.index_pages<<" pages of 4096 bytes\nRoot: "<<stats.root_page<<"\nOpening scanned records: "<<stats.records_scanned<<"\nRecovered tail: "<<stats.recovered_tail<<'\n';return 0;}
    if(command=="validate"&&(argc==3||argc==4)){const bool full=argc==4&&std::string_view(argv[3])=="full";if(argc==4&&!full&&std::string_view(argv[3])!="quick")throw std::invalid_argument("Validation mode must be quick or full");store.validate(full);std::cout<<"Valid NAVIER02 "<<(full?"full (all committed historical field checksums)":"quick (live metadata and index)")<<"\n";return 0;}
    if(command=="repair-tail"&&argc==3){store.repair_tail();std::cout<<"Incomplete tail repaired if present\n";return 0;}
    if((command=="checkpoint-json"||command=="fields"||command=="get")&&argc==4){const auto cp=store.get_checkpoint(unsigned_number(argv[3]));if(command=="checkpoint-json")checkpoint_json(std::cout,cp);else{std::cout<<"step="<<cp.step<<" time="<<cp.time<<" fields="<<cp.fields.size()<<'\n';for(const auto& f:cp.fields){std::cout<<f.name<<" "<<(f.scalar_type==storage::ScalarType::float32?"float32":"float64")<<" shape=";for(const auto n:f.dimensions)std::cout<<n<<' ';std::cout<<'\n';}}return 0;}
    if(command=="extract"&&(argc==5||argc==6)){const auto field=store.load_field(unsigned_number(argv[3]),argv[4]);if(argc==6){std::ofstream out(argv[5],std::ios::binary);if(!out)throw std::runtime_error("Cannot open extraction output");raw_field(out,field);}else{
#ifdef _WIN32
        if(_setmode(_fileno(stdout),_O_BINARY)==-1)throw std::runtime_error("Cannot set binary stdout");
#endif
        raw_field(std::cout,field);}return 0;}
    if((command=="range"&&argc==5)||(command=="list"&&argc==3)||(command=="list-json"&&argc==6)){
        const auto begin=command=="list"?0:unsigned_number(argv[3]),end=command=="list"?std::numeric_limits<std::uint64_t>::max():unsigned_number(argv[4]);const auto limit=command=="list-json"?format::memory_size(unsigned_number(argv[5])):1000U;const auto list=store.range_steps(begin,end,limit);
        if(command=="list-json"){std::cout<<"{\"checkpoints\":[";for(std::size_t i=0;i<list.size();++i){if(i)std::cout<<',';checkpoint_json(std::cout,list[i]);}std::cout<<"],\"nextStep\":";if(list.size()==limit&&!list.empty()&&list.back().step!=std::numeric_limits<std::uint64_t>::max())std::cout<<'"'<<list.back().step+1<<'"';else std::cout<<"null";std::cout<<'}';}else{for(const auto& cp:list)std::cout<<"step="<<cp.step<<" time="<<cp.time<<" fields="<<cp.fields.size()<<'\n';if(list.size()==limit)std::cout<<"Result limited to "<<limit<<" descriptors; use list-json pagination\n";}return 0;
    }
    if(command=="query"&&argc==6){const auto parse=[](const char* input){const std::string_view text(input);double value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);if(error!=std::errc{}||end!=text.data()+text.size())throw std::invalid_argument("Invalid metric bounds");return value;};query::QueryEngine engine(store);engine.build_metric_index(argv[3]);const auto result=engine.query_metric(argv[3],parse(argv[4]),parse(argv[5]));std::cout<<"RAM secondary B+ Tree: initial build scans metadata, not field payloads\n";for(const auto& cp:result){checkpoint_json(std::cout,cp);std::cout<<'\n';}return 0;}
    throw std::invalid_argument("Unsupported NAVIER02 command or argument count");
}
}
