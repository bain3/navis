#include "libs/httplib.h"
#include "libs/json.hpp"
#include "libs/sha1.h"
#include "libs/hmac.h"
#include "executor.h"
#include <filesystem>
#include "utils.h"

bool send_response(httplib::Response &res, const int &code, const std::string &message) {
    res.status = code;
    nlohmann::json body;
    body["status"] = code;
    body["detail"] = message;
    res.set_content(body.dump(), "application/json");
    utils::print_message(code == 200 ? "Success" : "Error (" + std::to_string(code) + ")", code == 200 ? 32 : 31,
                         message);
    return true;
}

int main([[maybe_unused]] int argc, char **argv) {
    httplib::Server svr;
    std::ifstream settings_file(argv[1]);
    if (!settings_file.is_open()) {
        utils::print_message("Critical", 31, "Unable to open settings file!");
        return 1;
    }
    nlohmann::json settings;
    settings_file >> settings;
    settings_file.close();

    svr.Post("/deploy", [&settings](const httplib::Request &req, httplib::Response &res,
                                    const httplib::ContentReader &content_reader) {

        // get request body
        std::string body;
        content_reader([&](const char *data, size_t data_length) {
            body.append(data, data_length);
            return true;
        });
        utils::print_message("Info", 36, "Received hook.");

        nlohmann::json json_body;
        try {
            json_body = nlohmann::json::parse(body);
        }
        catch (nlohmann::detail::parse_error &ex) {
            return send_response(res, 400, "Could not parse json.");
        }

        // check for event type
        if (!req.has_header("X-GitHub-Event"))
            return send_response(res, 400, "Missing X-GitHub-Event header");
        std::string event_type = req.get_header_value("X-GitHub-Event");
        if (event_type == "ping")
            return send_response(res, 200, "pong");
        else if (event_type != "deployment")
            return send_response(res, 400, "This deployment server only supports deployment events");

        // get repository settings
        if (!json_body.contains("/repository/full_name"_json_pointer))
            return send_response(res, 400, "Could not find repository name in request.");
        auto repo_name = json_body["/repository/full_name"_json_pointer].get<std::string>();
        nlohmann::json repo_settings;
        for (auto &element : settings["repos"]) {
            if (element["name"].get<std::string>() == repo_name) {
                repo_settings = element;
                break;
            }
        }
        if (repo_settings.is_null())
            return send_response(res, 404, "Repository not found in settings.");
        auto key = repo_settings["secret"].get<std::string>();

        // verify signature
        std::string signature;
        if (req.has_header("X-Hub-Signature")) {
            signature = req.get_header_value("X-Hub-Signature");
        }
        std::string sha1hmac = hmac<SHA1I>(body, key);
        sha1hmac.insert(0, "sha1=");
        if (sha1hmac != signature) {
            return send_response(res, 401, "Invalid signature.");
        }

        // get environment
        auto environment = json_body["deployment"]["environment"].get<std::string>();
        if (!repo_settings["environments"].contains(environment))
            return send_response(res, 404, "No script for this environment found.");
        if (!repo_settings.contains("token"))
            return send_response(res, 500, "Don't have a github token");

        // run deployment in another thread
        executor::DeploymentContext dc{
                .deployment_id=json_body["deployment"]["id"].get<int>(),
                .deployment_script=repo_settings["environments"][environment]["command"].get<std::string>(),
                .repo_name=repo_name,
                .ref=json_body["deployment"]["ref"].get<std::string>(),
                .environment=environment,
                .auto_inactive=repo_settings["environments"][environment].value("auto_inactive", false),
                .output_type=repo_settings["environments"][environment].value("output", "none")
        };
        std::thread deployment_thread(
                executor::deploy,
                repo_settings["token"].get<std::string>(),
                settings["public_hostname"].get<std::string>(),
                dc
        );
        deployment_thread.detach();

        return send_response(res, 200, "Deployment successfully scheduled");
    });

    svr.Get("/", [](const httplib::Request &req, httplib::Response &res) {
        // just a landing page
        res.set_content(
                R"(<html><body style="background: #221f1f; color: #fff; font-family: monospace;">
                        <div style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%);
                        text-align: center;">
                        <b>The naked man fears no pickpocket</b><br><i>Navis, a lightweight deployment server
                        </i></div></body></html>)",
                "text/html"
        );
    });
    int port;
    std::filesystem::create_directory("./logs");
    std::filesystem::create_directory("./output");
    auto ret = svr.set_mount_point("/logs", "./logs");
    ret |= svr.set_mount_point("/deployments", "./output");
    if (!ret) {
        utils::print_message("Critical", 31, "Logs or output directory does not exist and cannot be created...");
    }
    if (!settings.contains("port")) {
        port = svr.bind_to_any_port("0.0.0.0");
        utils::print_message("Info", 36, "Listening on port " + std::to_string(port));
        svr.listen_after_bind();
    } else {
        port = settings["port"].get<int>();
        utils::print_message("Info", 36, "Listening on port " + std::to_string(port));
        svr.listen("0.0.0.0", port);
    }

}