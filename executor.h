#ifndef NAVIS_EXECUTOR_H
#define NAVIS_EXECUTOR_H

#include <string>

namespace executor {

    struct DeploymentContext {
        int deployment_id;
        std::string deployment_script;
        std::string repo_name;
        std::string ref;
        std::string environment;
        bool auto_inactive;
        std::string output_type;
    };

    inline std::mutex datafile_mutex;

    void deploy(const std::string &token,
                const std::string &hostname,
                const DeploymentContext &context);

    std::string get_internal_deployment_id(const executor::DeploymentContext &context, const int &id);
}

#endif //NAVIS_EXECUTOR_H
