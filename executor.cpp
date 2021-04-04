#include <filesystem>
#include <mutex>
#include "executor.h"
#include "libs/httplib.h"
#include "libs/json.hpp"
#include "utils.h"
#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

void update_state(httplib::Client &c, const nlohmann::json &req,
                  const std::string &token, const int &deployment_id, const std::string &repo) {
    auto res = c.Post(("/repos/" + repo + "/deployments/" + std::to_string(deployment_id) + "/statuses").data(),
                      req.dump(),
                      "application/json");
    std::cout << req.dump() << std::endl;
    if (res.error() != 0 || res->status != 201) {
        utils::print_message("Error", 31, "Error while setting new state for " + std::to_string(deployment_id));
    } else {
        utils::print_message("Info", 36, "Successfully set new state for " + std::to_string(deployment_id));
    }
}

void mark_others_inactive(httplib::Client &c,
                          const executor::DeploymentContext &context,
                          const std::string &token,
                          const std::string &hostname,
                          const std::string &env_url) {
    // lock data file mutex
    std::lock_guard<std::mutex> lock(executor::datafile_mutex);

    // get data about current active deployments
    std::ifstream data_file_in("./data.json");
    if (!data_file_in.is_open()) {
        utils::print_message("Error", 31, "Cannot open data file for reading");
        return;
    }
    nlohmann::json active_data;
    data_file_in >> active_data;
    data_file_in.close();

    nlohmann::json req{};
    // mark current active deployment as inactive
    if (active_data.contains(""_json_pointer / context.repo_name / context.environment / "active")) {
        int active_id = active_data[""_json_pointer / context.repo_name / context.environment /
                                    "active"].get<int>();
        req["state"] = "inactive";
        req["log_url"] = hostname + "/logs/" + executor::get_internal_deployment_id(context, active_id);
        if (active_data.contains(""_json_pointer / context.repo_name / context.environment / "env_url")) {
            req["environment_url"] = active_data[""_json_pointer / context.repo_name / context.environment /
                                                 "env_url"].get<std::string>();
            active_data[context.repo_name][context.environment].erase("env_url");
        }
        update_state(c, req, token, active_id, context.repo_name);
    }
    active_data[""_json_pointer / context.repo_name / context.environment / "active"] = context.deployment_id;
    if (!env_url.empty()) {
        active_data[""_json_pointer / context.repo_name / context.environment / "env_url"] = env_url;
    }


    // overwrite file with new data
    std::ofstream data_file("./data.json");
    if (!data_file.is_open()) {
        utils::print_message("Error", 31, "Cannot open data file for writing");
        return;
    }
    data_file << active_data.dump();
    data_file.close();
}

void executor::deploy(const std::string &token, const std::string &hostname,
                      const executor::DeploymentContext &context) {

    httplib::Client c("https://api.github.com");
    c.set_default_headers({
                                  {"Authorization", ("Bearer " + token).data()},
                                  {"Accept",        "application/vnd.github.flash-preview+json"}
                          });

    std::string internal_deployment_id = get_internal_deployment_id(context, context.deployment_id);

    // create log file
    fs::path logfile("logs/" + internal_deployment_id);
    std::ofstream file(logfile);
    nlohmann::json req{};
    if (!file.is_open()) {
        req["state"] = "error";
        utils::print_message("Error", 31, "Cannot open a new log file");
        update_state(c, req, token, context.deployment_id, context.repo_name);
        return;
    }
    std::time_t result = std::time(nullptr);
    std::string t(ctime(&result));
    file.write(("-- " + t + "\n").data(), t.length() + 4);
    file.close();
    utils::print_message("Info", 36, "Log file created");
    // publish log file
    req["state"] = "in_progress";
    req["log_url"] = hostname + "/" + logfile.string();
    update_state(c, req, token, context.deployment_id, context.repo_name);

    // set environment vars
    fs::path output_file = fs::absolute(fs::path("output") / internal_deployment_id);
    setenv("NAVIS_LOGFILE", fs::absolute(logfile).c_str(), true);
    setenv("NAVIS_REF", context.ref.data(), true);
    setenv("NAVIS_REPONAME", context.repo_name.data(), true);
    setenv("NAVIS_OUTPUTFILE", output_file.c_str(), true);

    // run deployment script
    utils::print_message("Info", 36, "Executing script...");
    int exit_code = std::system(context.deployment_script.data());
    if (exit_code != 0) {
        req["state"] = "failure";
    } else {
        req["state"] = "success";
    }

    // add url if needed
    if (context.output_type == "file") {
        std::string file_name;
        for (auto &f : fs::directory_iterator(fs::path("output"))) {
            if (f.path().stem() == output_file.stem())
                file_name = f.path().filename();
        }
        if (!file_name.empty())
            req["environment_url"] = hostname + "/deployments/" + file_name;
    } else if (context.output_type == "url") {
        if (fs::exists(output_file)) {
            std::ifstream f(output_file);
            if (f.is_open()) {
                std::string url;
                if (std::getline(f, url)) {
                    req["environment_url"] = url;
                } else {
                    utils::print_message("Error", 31, "No url, output file empty");
                }
                f.close();
            } else {
                utils::print_message("Error", 31, "Cannot open deployment output file");
            }
            // remove the file
            fs::remove(output_file);
        }
    }

    if (context.auto_inactive)
        mark_others_inactive(c, context, token, hostname, req.value("environment_url", ""));

    // finally update the state online
    update_state(c, req, token, context.deployment_id, context.repo_name);
}

std::string executor::get_internal_deployment_id(const executor::DeploymentContext &context, const int &id) {
    std::string deployment_id_str = std::to_string(id);
    unsigned char digest[20];
    SHA1(reinterpret_cast<const unsigned char *>(
                 (context.repo_name + context.environment + deployment_id_str).data()),
         context.repo_name.size() + context.environment.size() + deployment_id_str.size(),
         reinterpret_cast<unsigned char *>(&digest));
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i : digest)
        ss << std::setw(2) << i;
    return ss.str();
}
