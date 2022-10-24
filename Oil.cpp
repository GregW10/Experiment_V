//
// Created by GregW on 14/03/2022.
//

#include "oilproc.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    if (argc == 1) {
        std::cerr << "Invalid number of arguments provided.\n";
        return 1;
    }
    std::string home_path = oil::get_home_path<std::string>();
#ifndef _WIN32
    char end[] = "/Exp_V_Program_Files/Oil_Reader";
    char prog_files[] = "/Exp_V_Program_Files/";
#else
    char end[] = "\\Exp_V_Program_Files\\Oil_Reader.exe";
    char prog_files[] = "\\Exp_V_Program_Files\\";
#endif
    std::string prog_files_path(home_path);
    prog_files_path.append(prog_files);
    std::string dat_file_path = prog_files_path + "All_Runs.dat";
    std::string constants(prog_files_path);
    constants.append("Constants.txt");
    std::string def_graph_vars_path = home_path + prog_files;
    def_graph_vars_path.append("GraphVariables.txt");
    std::string single_run_param_path = prog_files_path + "SingleRunParameters.txt";
    std::string dat_text_path = prog_files_path + "All_Runs.txt";
    bool show = false;
    oil::make_dir(prog_files_path);
    if (argc > 3) {
        if (strcmp(*(argv + 3), "show") == 0) {
            show = true;
        }
        else {
            fprintf(stderr, "Invalid number of arguments provided (%d) and/or fourth argument not \"show\".\n", argc - 1);
            exit(EXIT_FAILURE);
        }
    }
    else if(strcmp(*(argv + 1), "delete") == 0) {
        if (argc == 2) {
            int retval = oil::del(dat_file_path);
            if (retval == 1) {
                std::cerr << "Data file non-existent or not in expected location.\n";
            }
            return retval;
        }
        else {
            try {
                oil::Oil_run::delete_run(dat_file_path.c_str(), *(argv + 2));
            }
            catch (const std::invalid_argument &exception) {
                std::cerr << "Data file non-existent or not in expected location.\n";
                return 1;
            }
            return 0;
        }
    }
    else if(strcmp(*(argv + 1), "gentext") == 0) {
        int gen_ret = oil::Oil_run::gen_text(dat_file_path.c_str(), dat_text_path.c_str());
        if (gen_ret == 1) {
            std::cerr << "No .dat file found at the expected path: " << dat_file_path << '\n';
            std::cerr << "Run the program first to generate the .dat file.\n";
        }
        return gen_ret;
    }
    else if(strcmp(*(argv + 1), "gensample") == 0) {
        return oil::generate_sample_texts(constants.c_str(), def_graph_vars_path.c_str(),
                                          single_run_param_path.c_str());
    }
    char *freq_c = *(argv + 2);
    if (!oil::is_numeric(freq_c)) {
        fprintf(stderr, "The frequency that was input, %s, is not numeric.\n", freq_c);
        exit(EXIT_FAILURE);
    }
    unsigned long int freq = strtol(freq_c, nullptr, 10);
    struct stat buff = {};
    char *file_path;
    std::string latest_file;
    if (strcmp(*(argv + 1), "auto") == 0) {
        std::string downloads(home_path);
#ifndef _WIN32
        downloads.append("/Downloads");
#else
        downloads.append("\\Downloads");
#endif
        if (stat(downloads.c_str(), &buff) == -1) {
            fprintf(stderr, "\"Downloads\" folder does not exist. Please run program again and specify full "
                            "file path.\n");
            exit(EXIT_FAILURE);
        }
        struct dirent *entry = {};
        DIR *dir = opendir(downloads.c_str());

        if ((entry = readdir(dir)) == nullptr) {
            fprintf(stderr, "Downloads directory empty. Program exiting...\n");
            exit(EXIT_FAILURE);
        }
        entry = readdir(dir); entry = readdir(dir);
        latest_file = entry->d_name;
        fs::file_time_type time;
        std::time_t ftime = 0;
        fs::file_time_type new_time;
        std::time_t new_ftime;
        std::string not1 = ".";
        std::string not2 = "..";
        std::string new_file = "this_is_not_a_file";
        std::string full_latest_file;
        std::string full_new_file;
        while ((entry = readdir(dir)) != nullptr) {
            new_file = entry->d_name;
            size_t size = new_file.size();
            if (new_file.rfind(".csv", size - 4) != std::string::npos && new_file != not1 && new_file != not2) {
                if (latest_file.rfind(".csv", latest_file.size() - 4) ==  std::string::npos) {
                    latest_file = new_file;
                }
#ifndef _WIN32
                full_latest_file = downloads + "/" + latest_file;
                full_new_file = downloads + "/" + new_file;
#else
                full_latest_file = downloads + "\\" + latest_file;
                full_new_file = downloads + "\\" + new_file;
#endif
                time = fs::last_write_time(full_latest_file);
                new_time = fs::last_write_time(full_new_file);
                if (time <= new_time) {
                    latest_file = std::string(new_file);
                }
            }
        }
#ifndef _WIN32
        latest_file.insert(0, home_path + "/Downloads/");
#else
        latest_file.insert(0, home_path + "\\Downloads\\");
#endif
        file_path = (char *) malloc(latest_file.size() + 1);
        memset(file_path, 0, latest_file.size() + 1);
        strcpy(file_path, latest_file.c_str());
        std::cout << "\nTime period file: " << latest_file << "\n" << std::endl;
    }
    else {
#ifndef _WIN32
        file_path = (char *) malloc(PATH_MAX);
        realpath(*(argv + 1), file_path);
        std::memset(file_path, '\0', PATH_MAX);
        std::strcpy(file_path, *(argv + 1));
#else
        file_path = (char *) malloc(MAX_PATH);
        GetFullPathNameA(*(argv + 1), MAX_PATH, file_path, nullptr);
#endif
        file_path = (char *) realloc(file_path, strlen(file_path) + 1);
        if (stat(file_path, &buff) == -1) {
            perror("Error");
            exit(EXIT_FAILURE);
        }
        if (S_ISDIR(buff.st_mode)) {
            fprintf(stderr, "The file given is a directory. Program failure.");
            exit(EXIT_FAILURE);
        }
        latest_file = file_path;
    }
    if (latest_file.rfind(".csv", latest_file.size() - 4) == std::string::npos) {
        fprintf(stderr, "The file given/found is not a .csv file. Program aborting.\n");
        return 1;
    }
    free(file_path);
    oil::Oil_run run;
    std::string name;
    std::cout << "Set a name for this oil run (write \"file\" to use the time period file's name): ";
    std::cin >> name;
    oil::clear_cin();
    if (name == "file" || name == "FILE") {
        std::string::size_type dot_pos = latest_file.rfind('.');
        std::string::size_type slash_pos;
#ifndef _WIN32
        slash_pos = latest_file.rfind('/');
#else
        slash_pos = latest_file.rfind('\\');
#endif
        name = latest_file.substr(slash_pos + 1, dot_pos - slash_pos - 1);
    }
    run.set_name(name);
    std::string answer;
    std::cout << "\nConstants file path (specify \"auto\" to use the default path or 'q' to quit): ";
    std::cin >> answer;
    oil::clear_cin();
    oil::string_upper(answer);
    if (answer == "QUIT" || answer == "Q") {
        return 0;
    }
    int retval = oil::check_path(answer.c_str(), constants.c_str());
    if (retval == 0) {
        run.set_constants_path(answer);
    }
    else if (retval == 1) {
        return 1;
    }
    else {
        run.set_constants_path(constants);
    }
    run.read_constants();
    std::cout << "\nGraph variables file path (specify \"auto\" to use the default path or 'q' to quit): ";
    answer.clear();
    std::cin >> answer;
    oil::clear_cin();
    oil::string_upper(answer);
    if (answer == "QUIT" || answer == "Q") {
        return 0;
    }
    retval = oil::check_path(answer.c_str(), def_graph_vars_path.c_str());
    if (retval == 0) {
        run.read_graph_vars(answer.c_str());
    }
    else if (retval == 1) {
        return 1;
    }
    else {
        run.read_graph_vars(def_graph_vars_path.c_str());
    }
    std::string yes_no;
    std::cout << "\nCalculate viscosity from single run? [yes/no]: ";
    std::cin >> yes_no;
    oil::clear_cin();
    oil::string_upper(yes_no);
    while (yes_no != "YES" && yes_no != "NO") {
        std::cout << R"(Please only input "yes" or "no": )";
        std::cin >> yes_no;
        oil::clear_cin();
        oil::string_upper(yes_no);
    }
    bool yes = yes_no == "YES";
    if (yes) {
        std::cout << "\nSingle run parameters file path (specify \"auto\" to use the default path or 'q' to quit): ";
        answer.clear();
        std::cin >> answer;
        oil::clear_cin();
        oil::string_upper(answer);
        if (answer == "QUIT" || answer == "Q") {
            return 0;
        }
        retval = oil::check_path(answer.c_str(), single_run_param_path.c_str());
        if (retval == 0) {
            run.read_single_run_parameters(answer.c_str());
        } else if (retval == 1) {
            return 1;
        } else {
            run.read_single_run_parameters(single_run_param_path.c_str());
        }
    }
    std::string csv_file_path = prog_files_path + run[0] + ".csv";
    double *T;
    if (show) {
        T = run.get_T(latest_file.c_str(), 9, (double) freq, csv_file_path.c_str());
    }
    else {
        T = run.get_T(latest_file.c_str(), 9, (double) freq);
    }
    std::cout << "\nMaxima times and voltages:\n" << std::endl;
    run.display_V_t_map();
    std::cout << "\nAverage time period: " << *T << " +/- " << *(T + 1) << " seconds\n" << std::endl;
    free(T);
    double *visc_g = run.calc_visc_from_grad();
    std::cout << "The viscosity calculated from the gradient of the graph is: " << *visc_g << " +/- " << *(visc_g + 1)
              << oil::Oil_run::visc_units() << std::endl;
    free(visc_g);
    if (yes) {
        double *visc_s = run.calc_visc_from_T();
        std::cout << "The viscosity calculated from a single run is: " << *visc_s << " +/- " << *(visc_s + 1)
                  << oil::Oil_run::visc_units() << std::endl;
        free(visc_s);
    }
    std::string option;
    std::cout << "\nIn case a run with the same name exists, do you wish to overwrite, append, or do nothing? "
                 "[ow/app/dn]: ";
    std::cin >> option;
    oil::clear_cin();
    oil::string_upper(option);
    while (option != "OW" && option != "APP" && option != "DN") {
        std::cout << "You have not selected a valid option. Select again. [ow/app/dn]: ";
        std::cin >> option;
        oil::clear_cin();
        oil::string_upper(option);
    }
    run.write_data(dat_file_path.c_str(), option.c_str());
    if (show) {
        std::string cmd;
        cmd.append(home_path); cmd.append(end); cmd.append(" "); cmd.append(csv_file_path); cmd.append(" remove");
        std::string test_str(home_path);
        test_str.append(end);
        struct stat test = {};
        if (stat(test_str.c_str(), &test) == -1) {
#ifndef _WIN32
            fprintf(stderr, "\nThe \"Oil_Reader\" file is either non existent, or not where it needs to be: \n%s\n\n",
                    test_str.c_str());
#else
            fprintf(stderr, "\nThe \"Oil_Reader.exe\" file is either non existent, or not where it needs to be: "
                            "\n%s\n\n", test_str.c_str());
#endif
            exit(EXIT_FAILURE);
        }
        std::cout << "\nOil_Reader program found. Plotting graph...\n" << std::endl;
        system(cmd.c_str());
    }
    return 0;
}
