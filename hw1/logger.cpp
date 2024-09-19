#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unistd.h>

using namespace std;

int main(int argc, char* argv[]) {
    // handle arguments
    if (argc < 3) {
        cerr << "Usage: ./logger config.txt [-o file] [-p sopath] command [arg1 arg2 ...\n";
        return 1;
    }
    string config_file = argv[1];
    string output_file = "";
    string so_path = "";
    string command = "";
    vector<string> args;
    for (int i = 2; i < argc; i++) {
        if (string(argv[i]) == "-o") {
            output_file = argv[++i];
        } else if (string(argv[i]) == "-p") {
            so_path = argv[++i];
        } else {
            command = argv[i];
            for (int j = i + 1; j < argc; j++) {
                args.push_back(argv[j]);
            }
            break;
        }
    }
    if (so_path.empty()) {
        so_path = "./logger.so";
    }
    // cout << "config_file: " << config_file << endl;
    // cout << "output_file: " << output_file << endl;
    // cout << "so_path: " << so_path << endl;
    // cout << "command: " << command << endl;
    // cout << "args: ";
    // for (auto& arg : args) {
    //     cout << arg << " ";
    // }
    // set env
    char *envp[] = {NULL, NULL, NULL};
    envp[0] = (char*)malloc(100);
    snprintf(envp[0], 100, "LD_PRELOAD=%s", so_path.c_str());
    envp[1] = (char*)malloc(100);
    snprintf(envp[1], 100, "CONFIG=%s", config_file.c_str());
    envp[2] = (char*)malloc(100);
    snprintf(envp[2], 100, "OUTPUT=%s", output_file.c_str());
    envp[3] = NULL;
    // execute command
    // execve(command.c_str(), (char* const*)&new_args[0], envp);
    char* new_args[args.size() + 2];
    new_args[0] = (char*)command.c_str();
    for (int i = 0; i < static_cast<int>(args.size()); i++) {
        new_args[i + 1] = (char*)args[i].c_str();
    }
    new_args[args.size() + 1] = NULL;
    execve(command.c_str(), new_args, envp);
}