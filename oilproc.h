//
// Created by GregW on 01/04/2022.
//

#ifndef OILPROC_H
#define OILPROC_H

#define _USE_MATH_DEFINES

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdlib>
#include <cctype>
#include <deque>
#include <vector>
#include <iostream>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <regex>
#include <map>
#include <utility>
#include <cstring>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

#ifdef g
#undef g
#endif

constexpr double g = 9.80665;

namespace oil {

    class Oil_run {
    private:
        class FileFormatError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "The file format is not correct (could be due to excessively long lines).";
            }
        };
        class NoPathError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "You must set the path for constants to be read.";
            }
        };
        class NoConstantsError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "The read_constants() function must be called first.";
            }
        };
        class NoGradientError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "The read_grad_vars() function must be called first.";
            }
        };
        class NoTimePeriodError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "No time period has been supplied to this Oil_run object.";
            }
        };
        class FileWritingFailedError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "The file could not be written to or created. Please ensure you have provided a valid parent "
                       "directory path.";
            }
        };
        class FileReadingFailedError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "The file could not be read. Please ensure you have provided a valid path.";
            }
        };
        class NoMapError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "No map of max. V at times t found. Please call the get_T() function.";
            }
        };
        class NoSingleRunParameters : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "Single run parameters not found. Please call the read_single_run_parameters() function.";
            }
        };
        class NoNameError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "No name was found. Please name this object either using object[0] to access \"name\", or using the "
                       "set_name() function.";
            }
        };
        class LongNameError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return "Length of the name cannot be above 31 characters (excluding the NULL character).";
            }
        };
        class InvalidModeError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                return R"(Allowed modes are: "ow" (overwrite if exists), "app" (always append) or "dn" (do nothing if
                      exists).)";
            }
        };
        class DataFileSizeError : public std::exception {
            [[nodiscard]] const char *what() const noexcept {
                std::string exc = "Data file size not correct. Size must be: ";
                exc.append(std::to_string(sizeof(data)));
                exc.append(" bytes.");
                char *retval = (char *) malloc(exc.length() + 1);
                std::memset(retval, '\0', exc.length() + 1);
                std::strcpy(retval, exc.c_str());
                return retval;
            }
        };
        std::string constants;
        bool constants_read = false;
        bool grad_read = false;
        bool have_k = false;
        bool have_T = false;
        bool have_Vt = false;
        bool single_param_read = false;
        double c = 0;
        double c_err = 0;
        size_t max_name_size = 32;
        std::multimap<double, double> V_t;
        static size_t check_path(const char *path) {
            struct stat def_test = {};
            mode_t mode;
            if (stat(path, &def_test) == -1) {
                throw std::invalid_argument("Invalid file path provided.\n");
            }
            mode = def_test.st_mode;
            if (S_ISDIR(mode)) {
                throw std::invalid_argument("A path to a directory was provided.\n");
            }
            return def_test.st_size;
        }
        static size_t check_path(const std::string &path) {
            const char *path_c = path.c_str();
            return check_path(path_c);
        }
        static void check_line(const char *line_c) {
            std::regex rgx(R"(^[\w\s']+\s+=\s+-?\d+(\.\d+)?\s+#\s+[\w\d\^/\s-]+(\s+)?\r?\n?$)");
            if (!std::regex_match(line_c, rgx)) {
                throw FileFormatError();
            }
        }
        static void check_T_line(const char *line_c) {
            std::regex rgx(R"(^[^,]{1,101},[^,]{1,101},\s{0,21}\d{1,41}(\.(\d{1,41}))?,.{0,101}\r?\n?$)");
            if (!std::regex_match(line_c, rgx)) {
                throw FileFormatError();
            }
        }
        void check_if_name_present() const {
            if (std::strlen(run_data.name) == 0) {
                throw NoNameError();
            }
        }
        void check_name(const char *name) const {
            if (std::strlen(name) > max_name_size - 1) {
                throw LongNameError();
            }
        }
        static void read_path(const char *path, std::vector<double> &values, int num_lines) {
            std::ifstream file(path, std::fstream::in);
            if (!(file.is_open()) || file.fail()) {
                throw std::invalid_argument("Error opening file.");
            }
            std::string line;
            char *line_c_str = (char *) malloc(1);
            char *value_str;
            double value;
            int count = 0;
            while (getline(file, line)) {
                if (count > num_lines) {
                    throw FileFormatError();
                }
                check_line(line.c_str());
                line_c_str = (char *) realloc(line_c_str, line.length() + 1);
                strcpy(line_c_str, line.c_str());
                strtok(line_c_str, "=");
                value_str = strtok(nullptr, "=");
                value = strtod(value_str, nullptr);
                values.push_back(value);
                count++;
            }
            file.close();
            free(line_c_str);
        }
        static double mean_avg(const std::deque<double> &values) {
            size_t size = values.size();
            double total = 0;
            for (double value : values) {
                total += value;
            }
            double mean = total / (double) size;
            return mean;
        }
        static double SD(const std::deque<double> &values) {
            std::deque<double> squares;
            double square;
            for (double val : values) {
                squares.push_back(pow(val, 2));
            }
            double mean_of_squares = mean_avg(squares);
            double square_of_mean = pow(mean_avg(values), 2);
            return sqrt(mean_of_squares - square_of_mean);
        }
        static double *avg_time_diff(const std::deque<double> &times) {
            std::deque<double> diffs;
            size_t total = times.size() - 1;
            int count = 0;
            for (double time : times) {
                if (count > 0) {
                    diffs.push_back(time - times[count - 1]);
                }
                count++;
            }
            double total_diff = 0;
            for (double diff : diffs) {
                total_diff += diff;
            }
            double mean = total_diff / (double) total;
            double sd = SD(diffs);
            auto *retval = (double *) malloc(2*sizeof(double));
            *retval = mean;
            *(retval + 1) = sd;
            return retval;
        }
        static int discard_beg(const std::deque<double> &full, int first_max_position) {
            int result = 1;
            auto start_beg = full.begin();
            auto end_beg = full.begin() + first_max_position;
            auto start_rest = full.begin() + first_max_position + 1;
            auto end_rest = full.end();
            std::deque<double> beg(start_beg, std::next(end_beg));
            std::deque<double> rest(start_rest, std::next(end_rest));
            double avg_beg = mean_avg(beg);
            double avg_rest = mean_avg(rest);
            if (avg_beg < avg_rest) {
                result = 0;
            }
            return result;
        }
        static std::string string_upper(const char *str) {
            size_t length = std::strlen(str);
            std::string ret_string;
            for (int i = 0; i < length; i++) {
                ret_string.push_back((char) toupper(*(str + i)));
            }
            return ret_string;
        }
        typedef struct {
            char name[32];
            double a;
            double a_err;
            double b;
            double b_err;
            double drum;
            double drum_err;
            double MT_v_l_slope;
            double MT_v_l_slope_err;
            double intercept;
            double intercept_err;
            double k;
            double k_err;
            double mass;
            double mass_err;
            double T;
            double T_err;
            double submergence;
            double sub_err;
            double viscosity;
            double visc_err;
        } data;
        data run_data;
    public:
        Oil_run() {
            std::memset(run_data.name, '\0', max_name_size);
            run_data = {};
        }
        explicit Oil_run(const std::string &constants_path) {
            check_path(constants_path);
            constants.append(constants_path);
            std::memset(run_data.name, '\0', max_name_size);
            run_data = {};
        }
        explicit Oil_run(const char *constants_path) {
            check_path(constants_path);
            constants.append(constants_path);
            std::memset(run_data.name, '\0', max_name_size);
            run_data = {};
        }
        void set_constants_path(const std::string &constants_path) {
            constants_read = false;
            check_path(constants_path);
            constants.append(constants_path);
        }
        void set_constants_path(const char *constants_path) {
            constants_read = false;
            check_path(constants_path);
            constants.append(constants_path);
        }
        void set_name(const char *name) {
            check_name(name);
            std::strcpy(run_data.name, name);
        }
        void set_name(const std::string &name) {
            check_name(name.c_str());
            std::strcpy(run_data.name, name.c_str());
        }
        void read_constants() {
            if (constants.empty()) {
                throw NoPathError();
            }
            std::vector<double> values;
            read_path(constants.c_str(), values, 6);
            constants_read = true;
            run_data.a = values[0] / 2;
            run_data.a_err = values[1] / 2;
            run_data.b = values[2] / 2;
            run_data.b_err = values[3] / 2;
            run_data.drum = values[4];
            run_data.drum_err = values[5];
        }
        void read_graph_vars(const char *grad_vars_path) {
            check_path(grad_vars_path);
            std::string path = grad_vars_path;
            std::vector<double> values;
            read_path(path.c_str(), values, 4);
            run_data.MT_v_l_slope = values[0];
            run_data.MT_v_l_slope_err = values[1];
            run_data.intercept = values[2];
            run_data.intercept_err = values[3];
            grad_read = true;
            double c1 = (g*run_data.drum)/(8*pow(M_PI, 2)*pow(run_data.b, 2));
            double c2 = (g*run_data.drum)/(8*pow(M_PI, 2)*pow(run_data.a, 2));
            c = c1 - c2;
            double c_err_1 = c1*sqrt(pow(run_data.drum_err/run_data.drum, 2) + 4*pow(run_data.b_err/run_data.b, 2));
            double c_err_2 = c2*sqrt(pow(run_data.drum_err/run_data.drum, 2) + 4*pow(run_data.a_err/run_data.a, 2));
            c_err = sqrt(c_err_1*c_err_1 + c_err_2*c_err_2);
        }
        double *calc_visc_from_grad() {
            if (!(constants_read)) {
                throw NoConstantsError();
            }
            if (!(grad_read)) {
                throw NoGradientError();
            }
            auto *retval = (double *) malloc(2*sizeof(double));
            double eta = c*run_data.MT_v_l_slope;
            double eta_err = eta*sqrt(pow(c_err/c, 2) + pow(run_data.MT_v_l_slope_err/run_data.MT_v_l_slope, 2));
            run_data.viscosity = eta;
            run_data.visc_err = eta_err;
            *retval = eta;
            *(retval + 1) = eta_err;
            run_data.k = (c/eta)*run_data.intercept;
            run_data.k_err = run_data.k*sqrt(pow(c_err/c, 2) + pow(eta_err/eta, 2) +
                                             pow(run_data.intercept_err/run_data.intercept, 2));
            have_k = true;
            return retval;
        }
        void read_single_run_parameters(const char *path) {
            check_path(path);
            std::vector<double> values;
            read_path(path, values, 4);
            run_data.mass = values[0];
            run_data.mass_err = values[1];
            run_data.submergence = values[2];
            run_data.sub_err = values[3];
            single_param_read = true;
        }
        double *get_T(const char *path_c, int skip_lines, double freq, const char *write_path_c = nullptr) {
            check_path(path_c);
            if (write_path_c != nullptr) {
                std::string write_path(write_path_c);
            }
            std::string path(path_c);
            std::deque<double> all_times;
            std::deque<double> channel0;
            FILE *fp = fopen(path_c, "r");
            char *buffer = (char *) malloc(512*sizeof(char));
            int count = 0;
            char comma[2] = ",";
            while (fgets(buffer, 512, fp) != nullptr) {
                if (count > skip_lines) {
                    check_T_line(buffer);
                    strtok(buffer, comma);
                    double sample_time = strtod(buffer, nullptr) / freq;
                    all_times.push_back(sample_time);
                    strtok(nullptr, comma);
                    char *num = strtok(nullptr, comma);
                    double channel0_num = strtod(num, nullptr);
                    channel0.push_back(channel0_num);
                }
                count++;
            }
            fclose(fp);
            free(buffer);
            if (write_path_c != nullptr) {
                FILE *toWrite = fopen(write_path_c, "w+");
                if (toWrite == nullptr) {
                    throw FileWritingFailedError();
                }
                int count_w = 0;
                std::string writing;
                for (double time: all_times) {
                    writing = std::to_string(time) + "," + std::to_string(channel0[count_w]) + "\n";
                    fputs(writing.c_str(), toWrite);
                    count_w++;
                }
                fclose(toWrite);
            }
            double mean_v = mean_avg(channel0);
            double big = 0;
            std::deque<double> maxima;
            std::deque<double> maxima_times;
            int count_v = 0;
            bool first_time = true;
            int first_max_pos;
            for (double voltage : channel0) {
                if (voltage > big && voltage > mean_v) {
                    big = voltage;
                }
                if (voltage < mean_v) {
                    if (big != 0 && big != voltage) {
                        maxima.push_back(big);
                        big = 0;
                        maxima_times.push_back(all_times[count_v]);
                        if (first_time) {
                            first_max_pos = count_v;
                            first_time = false;
                        }
                    }
                }
                count_v++;
            }
            if (discard_beg(channel0, first_max_pos)) {
                maxima.erase(maxima.cbegin());
                maxima_times.erase(maxima_times.cbegin());
            }
            maxima.pop_front(); maxima.pop_front();
            maxima_times.pop_front(); maxima_times.pop_front();
            int count_final = 0;
            for (double volt : maxima) {
                V_t.insert({maxima_times[count_final], volt});
                count_final++;
            }
            have_Vt = true;
            double *both = avg_time_diff(maxima_times);
            run_data.T = *both;
            run_data.T_err = *(both + 1);
            have_T = true;
            return both;
        }
        double *calc_visc_from_T() {
            if (!have_T) {
                throw NoTimePeriodError();
            }
            if (!single_param_read) {
                throw NoSingleRunParameters();
            }
            auto *retval = (double *) malloc(2*sizeof(double));
            double h = run_data.submergence + run_data.k;
            double h_err = sqrt(run_data.sub_err*run_data.sub_err + run_data.k_err*run_data.k_err);
            run_data.viscosity = c*((run_data.mass*run_data.T)/h);
            run_data.visc_err = run_data.viscosity*sqrt(pow(c_err/c, 2) + pow(run_data.mass_err/run_data.mass, 2) +
                                                        pow(run_data.T_err/run_data.T, 2) + pow(h_err/h, 2));
            *retval = run_data.viscosity;
            *(retval + 1) = run_data.visc_err;
            return retval;
        }
        std::multimap<double, double> get_max_V_t_map() {
            if (!have_Vt) {
                throw NoMapError();
            }
            return V_t;
        }
        void display_V_t_map() {
            if (!have_Vt || !have_T) {
                throw NoMapError();
            }
            for (const std::pair<const double, double> &pair : V_t) {
                std::cout << pair.second << " volts at time: " << pair.first << " s" << std::endl;
            }
        }
        int write_data(const char *path_c, const char *mode_c = "OW") {
            check_if_name_present();
            std::string path(path_c);
            std::string mode;
            mode = string_upper(mode_c);
            if (path.rfind(".dat", path.size() - 4) == std::string::npos) {
                throw std::invalid_argument("Data can only be written to a .dat file.");
            }
            if (mode != "OW" && mode != "APP" && mode != "DN") {
                throw InvalidModeError();
            }
            struct stat file = {};
            if (stat(path_c, &file) == -1) {
                std::ofstream stream(path_c, std::fstream::out | std::fstream::trunc | std::fstream::binary);
                if (!stream.good()) {
                    throw FileWritingFailedError();
                }
                stream.write((char *) &run_data, sizeof(data));
                stream.close();
                return 0;
            }
            if (file.st_size == 0 || mode == "APP") {
                perhaps: {
                std::ofstream stream(path_c, std::fstream::out | std::fstream::binary | std::fstream::app);
                if (!stream.good()) {
                    throw FileWritingFailedError();
                }
                stream.write((char *) &run_data, sizeof(data));
                stream.close();
                return 1;
            };
            }
            std::ifstream stream(path_c, std::fstream::in | std::fstream::binary);
            if (!stream.good()) {
                throw FileWritingFailedError();
            }
            size_t file_size = file.st_size;
            unsigned long int num_structs = file_size / sizeof(data);
            data *runs = (data *) malloc(file_size);
            stream.read((char *) runs, (std::streamsize) file_size);
            stream.close();
            bool ow = false;
            int ow_count = 0;
            for (int i = 0; i < num_structs; i++) {
                if (std::strcmp(run_data.name, (runs + i)->name) == 0) {
                    if (mode == "DN") {
                        return 3;
                    }
                    else if (mode == "OW") {
                        *(runs + i) = run_data;
                        ow_count++;
                    }
                }
            }
            if (ow_count > 0) {
                std::ofstream stream_w(path_c, std::fstream::out | std::fstream::binary | std::fstream::trunc);
                if (!stream.good()) {
                    throw FileWritingFailedError();
                }
                stream_w.write((char *) runs, (std::streamsize) file_size);
                stream_w.close();
                free(runs);
                return 2;
            }
            else {
                free(runs);
                goto perhaps;
            }
        }
        static int delete_run(const char *path, const char *run_name) {
            struct stat def_test = {};
            mode_t mode;
            if (stat(path, &def_test) == -1) {
                throw std::invalid_argument("Invalid file path provided.\n");
            }
            mode = def_test.st_mode;
            if (S_ISDIR(mode)) {
                throw std::invalid_argument("A path to a directory was provided.\n");
            }
            size_t size = def_test.st_size;
            size_t length = std::strlen(path);
            const char *end = path + length - 4;
            if (std::strcmp(end, ".dat") != 0) {
                throw std::invalid_argument("A .dat file was not provided.");
            }
            std::ifstream input(path, std::fstream::in | std::fstream::binary);
            if (!input.good()) {
                throw FileReadingFailedError();
            }
            unsigned long num_structs = size / sizeof(data);
            data *runs = (data *) malloc(size);
            input.read((char *) runs, (std::streamsize) size);
            input.close();
            std::ofstream output(path, std::fstream::out | std::fstream::binary | std::fstream::trunc);
            if (!output.good()) {
                throw FileReadingFailedError();
            }
            for (int i = 0; i < num_structs; i++) {
                if (std::strcmp(run_name, (runs + i)->name) != 0) {
                    output.write((char *) (runs + i), sizeof(data));
                }
            }
            output.close();
            free(runs);
            return 0;
        }
        void load_from_dat(const char *dat_file_path) {
            size_t size = check_path(dat_file_path);
            if (size != sizeof(data)) {
                throw DataFileSizeError();
            }
            std::ifstream in(dat_file_path);
            *this << in;
            in.close();
        }
        static int gen_text(const char *input_path_c, const char *output_path_c, bool open = true) {
            std::string input_path(input_path_c);
            if (input_path.rfind(".dat", input_path.size() - 4) == std::string::npos) {
                throw std::invalid_argument("Data can only be read from a .dat file.");
            }
            struct stat info = {};
            stat(input_path_c, &info);
            if (info.st_size == 0) {
                return 1;
            }
            std::ifstream input_file(input_path_c, std::fstream::in | std::fstream::binary);
            if (!input_file.good()) {
                throw FileReadingFailedError();
            }
            std::ofstream output_file(output_path_c, std::fstream::trunc | std::fstream::out);
            if (!output_file.good()) {
                throw FileWritingFailedError();
            }
            size_t num_structs = info.st_size / sizeof(data);
            oil::Oil_run run;
            for (int i = 0; i < num_structs; i++) {
                run << input_file;
                run >> output_file;
                output_file << "\n\n" << std::endl;
            }
            input_file.close();
            output_file.close();
            if (open) {
                std::string command;
#ifndef _WIN32
                command.append("open ");
#endif
                command.append(output_path_c);
                system(command.c_str());
            }
            return 0;
        }
        [[nodiscard]] [[maybe_unused]] data get_data_struct_cp() const {
            return run_data;
        }
        [[nodiscard]] static std::string visc_units() {
            std::string units = " kg m^-1 s^-1";
            return units;
        }
        static void print_member_names() {
            std::cout << "name\na\na_err\nb\nb_err\ndrum\ndrum_err\nMT_v_l_slope\nMT_v_l_slope_err\nintercept\nintercept_er"
                      << "r\nk\nk_err\nmass\nmass_err\nT\nT_err\nsubmergence\nsub_err\nviscosity\nvisc_err" << std::endl;
        }
        double *operator[](const char *element) {
            std::string elem = string_upper(element);
            if (elem == "A") {
                return &run_data.a;
            }
            else if (elem == "A_ERR") {
                return &run_data.a_err;
            }
            else if (elem == "B") {
                return &run_data.b;
            }
            else if (elem == "B_ERR") {
                return &run_data.b_err;
            }
            else if (elem == "DRUM") {
                return &run_data.drum;
            }
            else if (elem == "DRUM_ERR") {
                return &run_data.drum_err;
            }
            else if (elem == "MT" || elem == "MT_V_L" || elem == "MT_V_L_SLOPE") {
                return &run_data.MT_v_l_slope;
            }
            else if (elem == "MT_ERR" || elem == "MT_V_L_ERR" || elem == "MT_V_L_SLOPE_ERR") {
                return &run_data.MT_v_l_slope_err;
            }
            else if (elem == "INTERCEPT") {
                return &run_data.intercept;
            }
            else if (elem == "INTERCEPT_ERR") {
                return &run_data.intercept_err;
            }
            else if (elem == "K") {
                return &run_data.k;
            }
            else if (elem == "K_ERR") {
                return &run_data.k_err;
            }
            else if (elem == "M" || elem == "MASS") {
                return &run_data.mass;
            }
            else if (elem == "M_ERR" || elem == "MASS_ERR") {
                return &run_data.mass_err;
            }
            else if (elem == "T") {
                return &run_data.T;
            }
            else if (elem == "T_ERR") {
                return &run_data.T_err;
            }
            else if (elem == "SUB" || elem == "SUBMERGENCE") {
                return &run_data.submergence;
            }
            else if (elem == "SUB_ERR" || "SUBMERGENCE_ERR") {
                return &run_data.sub_err;
            }
            else if (elem == "V" || elem == "VISC" || elem == "VISCOSITY") {
                return &run_data.viscosity;
            }
            else if (elem == "V_ERR" || elem == "VISC_ERR" || elem == "VISCOSITY_ERR") {
                return &run_data.visc_err;
            }
            else {
                throw std::out_of_range("The element you are indexing does not exist (use integer index '0' to access "
                                        "the \"name\" member.");
            }
        }
        char *operator[](int index) {
            if (index == 0) {
                return run_data.name;
            }
            else {
                throw std::out_of_range("You are indexing an element that does not exist.");
            }
        }
        std::ostream &operator>>(std::ostream &out) {
            out << "Name: " << run_data.name << "\n"
                << "Outer cylinder radius = " << run_data.a << " +/- " << run_data.a_err << " m\n"
                << "Inner cylinder radius = " << run_data.b << " +/- " << run_data.b_err << " m\n"
                << "Drum diameter = " << run_data.drum << " +/- " << run_data.drum_err << " m\n"
                << "MT vs l slope = " << run_data.MT_v_l_slope << " +/- " << run_data.MT_v_l_slope_err << " kg s m^-1\n"
                << "MT vs l intercept = " << run_data.intercept << " +/- " << run_data.intercept_err << " kg s\n"
                << "k correction factor = " << run_data.k << " +/- " << run_data.k_err << " m\n"
                << "Mass on balances = " << run_data.mass << " +/- " << run_data.mass_err << " kg\n"
                << "Time period = " << run_data.T << " +/- " << run_data.T_err << " s\n"
                << "Submergence = " << run_data.submergence << " +/- " << run_data.sub_err << " m\n"
                << "Viscosity = " << run_data.viscosity << " +/- " << run_data.visc_err << visc_units();
            return out;
        }
        Oil_run &operator<<(std::istream &in) {
            in.read((char *) &run_data, sizeof(data));
            return *this;
        }
        Oil_run &operator<<(data runData) {
            strcpy(this->run_data.name, runData.name);
            this->run_data.a = runData.a; this->run_data.a_err = runData.a_err;
            this->run_data.b = runData.b; this->run_data.b_err = runData.b_err;
            this->run_data.drum = runData.drum; this->run_data.drum_err = runData.drum_err;
            this->run_data.MT_v_l_slope = runData.MT_v_l_slope; this->run_data.MT_v_l_slope_err = runData.MT_v_l_slope_err;
            this->run_data.intercept = runData.intercept; this->run_data.intercept_err = runData.intercept_err;
            this->run_data.k = runData.k; this->run_data.k_err = runData.k_err;
            this->run_data.mass = runData.mass; this->run_data.mass_err = runData.mass_err;
            this->run_data.T = runData.T; this->run_data.T_err = runData.T_err;
            this->run_data.submergence = runData.submergence; this->run_data.sub_err = runData.sub_err;
            this->run_data.viscosity = runData.viscosity; this->run_data.visc_err = runData.visc_err;
            return *this;
        }
    };

    std::ostream &operator<<(std::ostream &out, oil::Oil_run run) {
        return run >> out;
    }

    Oil_run &operator>>(std::istream &in, oil::Oil_run run) {
        return run << in;
    }

    void string_upper(std::string &str) {
        for (char &ch : str) {
            ch = (char) std::toupper(ch);
        }
    }

    void string_upper(char *str) {
        size_t length = std::strlen(str);
        for (int i = 0; i < length; i++) {
            *(str + i) = (char) std::toupper(*(str + i));
        }
    }

    bool is_numeric(const char *str) {
        size_t length = std::strlen(str);
        bool is_num = true;
        for (int i = 0; i < length; i++) {
            if (isdigit(*(str + i)) == 0) {
                is_num = false;
                break;
            }
        }
        return is_num;
    }

    bool is_numeric(const std::string &str) {
        bool is_num = true;
        for (const char &ch : str) {
            if (isdigit(ch) == 0) {
                is_num = false;
                break;
            }
        }
        return is_num;
    }

    void clear_cin() {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    template <typename PATH>
    PATH get_home_path() {
        PATH retval = nullptr;
        return retval;
    }

    template <> char *get_home_path<char *>() {
#ifdef _WIN32
    char *home_path_c = (char *) malloc(MAX_PATH);
    HRESULT result = SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, home_path_c);
    if (result != S_OK) {
        return nullptr;
    }
    home_path_c = (char *) realloc(home_path_c, strlen_c(home_path_c) + 1);
    return home_path_c;
#else
    struct passwd *pwd;
    uid_t uid = getuid();
    pwd = getpwuid(uid);
    char *home_path_c = pwd->pw_dir;
    char *retval;
    retval = (char *) malloc(sizeof(char) * strlen(home_path_c) + 1);
    memset(retval, '\0', strlen(home_path_c) + 1);
    strcpy(retval, home_path_c);
    return retval;
#endif
}

    int check_path(const char *path_c, const char *def_path) {
        struct stat def_test = {};
        mode_t mode;
        std::string path(path_c);
        if (path != "AUTO") {
            if (stat(path_c, &def_test) == -1) {
                fprintf(stderr, "Invalid file path provided. Program aborting.\n");
                return 1;
            }
            mode = def_test.st_mode;
            if (S_ISDIR(mode)) {
                fprintf(stderr, "A path to a directory was provided. Program aborting.\n");
                return 1;
            }
        }
        else {
            if (stat(def_path, &def_test) == -1) {
                fprintf(stderr, "File not found at the default path: %s. Program aborting.\n", def_path);
                return 1;
            }
            mode = def_test.st_mode;
            if (S_ISDIR(mode)) {
                fprintf(stderr, "The default path is a directory: %s. Program aborting.\n", def_path);
                return 1;
            }
            return -1;
        }
        return 0;
    }
    int generate_sample_texts(const char *constants_path, const char *graph_vars_path,
                              const char *single_run_params_path) {
        FILE *fp1 = fopen(constants_path, "w+");
        FILE *fp2 = fopen(graph_vars_path, "w+");
        FILE *fp3 = fopen(single_run_params_path, "w+");
        if (fp1 == nullptr || fp2 == nullptr || fp3 == nullptr) {
            perror("Error");
            fprintf(stderr, "One or more files could not be written to and/or created. Please check parent directory "
                            "path and/or file permissions.");
            return 1;
        }
        const char const_w[] = "Outer cylinder diameter = 0.05373\t\t\t# m\n"
                               "Outer cylinder diameter error = 0.000005\t\t# m\n"
                               "Inner cylinder diameter = 0.03769\t\t\t# m\n"
                               "Inner cylinder diameter error = 0.000005\t\t# m\n"
                               "Diameter of drum = 0.019\t\t\t\t# m\n"
                               "Diameter of drum error = 0.000005\t\t\t# m";
        const char graph_w[] = "MT vs l gradient = 0.375\t\t# kg s m^-1\n"
                               "Gradient error = 0.005\t\t# kg s m^-1\n"
                               "MT vs l intercept = 0.00290\t# kg s\n"
                               "Intercept error = 0.000290\t# kg s";
        const char singleW[] = "Mass on scales = 0.020\t\t# kg\n"
                               "Mass error = 0.0001\t\t# kg\n"
                               "Submergence = 0.05\t\t# m\n"
                               "Submergence error = 0.0005\t# m";
        fprintf(fp1, const_w); fprintf(fp2, graph_w); fprintf(fp3, singleW); fclose(fp1); fclose(fp2); fclose(fp3);
        return 0;
    }
    int del(const char *path) {
        struct stat test = {};
        if (stat(path, &test) == -1) {
            return 1;
        }
        mode_t mode = test.st_mode;
        if (S_ISDIR(mode)) {
            return 1;
        }
        if (std::remove(path) == -1) {
            std::perror("The following error occurred");
            return 1;
        }
        return 0;
    }
    int del(const std::string &path) {
        return del(path.c_str());
    }
    int make_dir(const char *path) {
        struct stat test = {};
        if (stat(path, &test) == -1) {
            int retval;
#ifndef _WIN32
            retval = mkdir(path, S_IRWXG | S_IRWXO | S_IRWXU);
#else
            retval = mkdir(path);
#endif
            return retval;
        }
        return 1;
    }
    int make_dir(const std::string &path) {
        return make_dir(path.c_str());
    }
}
#endif
