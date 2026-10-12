#include "solvers/fluid3d.hpp"
#include "solvers/run_options.hpp"
#include <numeric>
int main(int argc,char** argv) {
    try {
        auto run=navier::examples::options(argc,argv,"fluid3d.data");
        if(argc<3)run.side=64;
        if(argc<4)run.steps=10000;
        if(argc<5)run.interval=100;
        navier::examples::Fluid3D solver(run.side);
        auto sidecar=run.path;sidecar+=".json";
        if(std::filesystem::exists(sidecar))throw std::runtime_error("Metadata output already exists");
        std::ofstream meta(sidecar);
        meta << "{\"solver\":\"abc-flow3d-d3q19\",\"nx\":" << run.side << ",\"ny\":" << run.side
             << ",\"nz\":" << run.side << ",\"dt\":1,\"steps\":" << run.steps
             << ",\"save_every\":" << run.interval << ",\"coefficient\":0.02,\"boundary\":\"periodic\","
             << "\"layout\":\"planar, index=(z*ny+y)*nx+x\",\"fields\":[\"density\",\"velocity_x\",\"velocity_y\",\"velocity_z\"]}";
        meta.close();if(!meta)throw std::runtime_error("Cannot write 3D metadata");
        navier::storage::CheckpointStore store(run.path);store.save(0,0,solver.values());
        for(std::uint64_t step=1;step<=run.steps;++step) { solver.advance();if(step%run.interval==0||step==run.steps) {
            const auto values=solver.values();store.save(step,static_cast<double>(step),values);
            if(step%100==0||step==run.steps)std::cout << "3D step=" << step << std::endl;
        }}
        navier::examples::demonstrate(store,run);return 0;
    } catch(const std::exception& error) { std::cerr << "Fluid3D: " << error.what() << '\n';return 1; }
}
