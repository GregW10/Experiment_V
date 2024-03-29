//
// Created by GregW on 01/04/2022.
//

#ifndef OILPROC_H
#define OILPROC_H

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

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
            [[nodiscard]] const char *what() const noexcept override {
                return "The file format is not correct (could be due to excessively long lines).";
            }
        };
        class NoPathError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "You must set the path for constants to be read.";
            }
        };
        class NoConstantsError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "The read_constants() function must be called first.";
            }
        };
        class NoGradientError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "The read_grad_vars() function must be called first.";
            }
        };
        class NoTimePeriodError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "No time period has been supplied to this Oil_run object.";
            }
        };
        class FileWritingFailedError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "The file could not be written to or created. Please ensure you have provided a valid parent "
                       "directory path.";
            }
        };
        class FileReadingFailedError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "The file could not be read. Please ensure you have provided a valid path.";
            }
        };
        class NoMapError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "No map of max. V at times t found. Please call the get_T() function.";
            }
        };
        class NoSingleRunParameters : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "Single run parameters not found. Please call the read_single_run_parameters() function.";
            }
        };
        class NoNameError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "No name was found. Please name this object either using object[0] to access \"name\", or using the "
                       "set_name() function.";
            }
        };
        class LongNameError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return "Length of the name cannot be above 31 characters (excluding the NULL character).";
            }
        };
        class InvalidModeError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                return R"(Allowed modes are: "ow" (overwrite if exists), "app" (always append) or "dn" (do nothing if
                      exists).)";
            }
        };
        class DataFileSizeError : public std::exception {
            [[nodiscard]] const char *what() const noexcept override {
                std::string exc = "Data file size not correct. Size must be: ";
                exc.append(std::to_string(sizeof(data)));
                exc.append(" bytes.");
                char *retval = (char *) malloc(exc.length() + 1);
                std::strcpy(retval, exc.c_str());
                return retval;
            }
        };
        std::string constants;
        bool constants_read = false;
        bool grad_read = false;
        bool have_T = false;
        bool have_Vt = false;
        bool single_param_read = false;
        double c = 0;
        double c_err = 0;
        size_t max_name_size = 32;
        std::multimap<double, double> V_t;
        static size_t check_path(const char *path) {
            struct stat def_test = {};
            if (stat(path, &def_test) == -1) {
                throw std::invalid_argument("Invalid file path provided.\n");
            }
            if (S_ISDIR(def_test.st_mode)) {
                throw std::invalid_argument("A path to a directory was provided.\n");
            }
            return def_test.st_size;
        }
        static size_t check_path(const std::string &path) {
            return check_path(path.c_str());
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
            if (!file.is_open() || !file.good()) {
                throw std::invalid_argument("Error opening file.\n");
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
            double total = 0;
            for (const double &value : values) {
                total += value;
            }
            return total / (double) values.size();
        }
        static double SD(const std::deque<double> &values) {
            std::deque<double> squares;
            for (const double &val : values) {
                squares.push_back(val*val);
            }
            double mean = mean_avg(values);
            return sqrt(mean_avg(squares) - mean*mean);
        }
        static double *avg_time_diff(const std::deque<double> &times) {
            std::deque<double> diffs;
            auto end = times.end();
            for (auto prev_it = times.cbegin(), it = prev_it + 1; it != end; ++prev_it, ++it) {
                diffs.push_back(*it - *prev_it);
            }
            double total_diff = 0;
            for (const double &diff : diffs) {
                total_diff += diff;
            }
            double *retval = (double *) malloc(2*sizeof(double));
            *retval = total_diff / (double) (times.size() - 1); // mean
            *(retval + 1) = SD(diffs); // standard deviation
            return retval;
        }
        static int discard_beg(const std::deque<double> &full, int first_max_position) {
            auto start_beg = full.begin();
            auto end_beg = start_beg + first_max_position;
            auto start_rest = end_beg + 1;
            auto end_rest = full.end();
            std::deque<double> beg(start_beg, std::next(end_beg));
            std::deque<double> rest(start_rest, std::next(end_rest));
            if (mean_avg(beg) < mean_avg(rest)) {
                return 0;
            }
            return 1;
        }
        static std::string string_upper(const char *str) {
            if (str == nullptr) {
                return {};
            }
            std::string ret_string;
            while (*str) {
                ret_string.push_back((char) toupper(*str++));
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
        data run_data{};
    public:
        Oil_run() {
            std::memset(run_data.name, '\0', max_name_size);
        }
        explicit Oil_run(const std::string &constants_path) {
            check_path(constants_path);
            constants.append(constants_path);
            std::memset(run_data.name, '\0', max_name_size);
        }
        explicit Oil_run(const char *constants_path) {
            check_path(constants_path);
            constants.append(constants_path);
            std::memset(run_data.name, '\0', max_name_size);
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
                for (const double &time: all_times) {
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
            for (const double &voltage : channel0) {
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
            for (const double &volt : maxima) {
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
        std::multimap<double, double> get_max_V_t_map() const {
            if (!have_Vt) {
                throw NoMapError();
            }
            return V_t;
        }
        void display_V_t_map() const {
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
                perhaps:
                std::ofstream stream(path_c, std::fstream::out | std::fstream::binary | std::fstream::app);
                if (!stream.good()) {
                    throw FileWritingFailedError();
                }
                stream.write((char *) &run_data, sizeof(data));
                stream.close();
                return 1;
            }
            std::ifstream stream(path_c, std::fstream::in | std::fstream::binary);
            if (!stream.good()) {
                throw FileWritingFailedError();
            }
            size_t file_size = file.st_size;
            size_t num_structs = file_size / sizeof(data);
            data *runs = (data *) malloc(file_size);
            stream.read((char *) runs, (std::streamsize) file_size);
            stream.close();
            size_t ow_count = 0;
            data *ptr = runs;
            for (size_t i = 0; i < num_structs; ++i, ++ptr) {
                if (std::strcmp(run_data.name, ptr->name) == 0) {
                    if (mode == "DN") {
                        return 3;
                    }
                    else if (mode == "OW") {
                        *ptr = run_data;
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
            if (stat(path, &def_test) == -1) {
                throw std::invalid_argument("Invalid file path provided.\n");
            }
            if (S_ISDIR(def_test.st_mode)) {
                throw std::invalid_argument("A path to a directory was provided.\n");
            }
            const char *end = path + std::strlen(path) - 4;
            if (std::strcmp(end, ".dat") != 0) {
                throw std::invalid_argument("A .dat file was not provided.\n");
            }
            std::ifstream input(path, std::fstream::in | std::fstream::binary);
            if (!input.good()) {
                throw FileReadingFailedError();
            }
            size_t num_structs = def_test.st_size / sizeof(data);
            data *runs = (data *) malloc(def_test.st_size);
            input.read((char *) runs, (std::streamsize) def_test.st_size);
            input.close();
            std::ofstream output(path, std::fstream::out | std::fstream::binary | std::fstream::trunc);
            if (!output.good()) {
                throw FileReadingFailedError();
            }
            data *ptr = runs;
            for (size_t i = 0; i < num_structs; i++, ++ptr) {
                if (std::strcmp(run_name, ptr->name) != 0) {
                    output.write((char *) ptr, sizeof(data));
                }
            }
            output.close();
            free(runs);
            return 0;
        }
        void load_from_dat(const char *dat_file_path) {
            if (check_path(dat_file_path) != sizeof(data)) {
                throw DataFileSizeError();
            }
            std::ifstream in(dat_file_path);
            in >> *this;
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
            for (size_t i = 0; i < num_structs; i++) {
                input_file >> run;
                output_file << run;
                output_file << "\n\n\n";
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
        [[nodiscard]] static const char *visc_units() {
            static const char units[] = " kg m^-1 s^-1";
            return units;
        }
        static void print_member_names() {
            std::cout << "name\na\na_err\nb\nb_err\ndrum\ndrum_err\nMT_v_l_slope\nMT_v_l_slope_err\nintercept\n"
                         "intercept_err\nk\nk_err\nmass\nmass_err\nT\nT_err\nsubmergence\nsub_err\nviscosity\nvisc_err"
                      << std::endl;
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
        char *operator[](const int &index) {
            if (index == 0) {
                return run_data.name;
            }
            else {
                throw std::out_of_range("You are indexing an element that does not exist.");
            }
        }
        char *operator[](const int &&index) {
            return (*this)[index];
        }
        friend std::ostream &operator<<(std::ostream &out, const Oil_run &run);
        friend Oil_run &operator>>(std::istream &in, Oil_run &run);
        friend Oil_run &operator>>(const data &runData, Oil_run &run);
    };

    std::ostream &operator<<(std::ostream &out, const oil::Oil_run &run) {
        return out
                << "Name: " << run.run_data.name << "\n"
                << "Outer cylinder radius = " << run.run_data.a << " +/- " << run.run_data.a_err << " m\n"
                << "Inner cylinder radius = " << run.run_data.b << " +/- " << run.run_data.b_err << " m\n"
                << "Drum diameter = " << run.run_data.drum << " +/- " << run.run_data.drum_err << " m\n"
                << "MT vs l slope = " << run.run_data.MT_v_l_slope << " +/- " << run.run_data.MT_v_l_slope_err
                << " kg s m^-1\n" << "MT vs l intercept = " << run.run_data.intercept << " +/- "
                << run.run_data.intercept_err << " kg s\n" << "k correction factor = " << run.run_data.k << " +/- "
                << run.run_data.k_err << " m\n" << "Mass on balances = " << run.run_data.mass << " +/- "
                << run.run_data.mass_err << " kg\n" << "Time period = " << run.run_data.T << " +/- "
                << run.run_data.T_err << " s\n" << "Submergence = " << run.run_data.submergence << " +/- "
                << run.run_data.sub_err << " m\n" << "Viscosity = " << run.run_data.viscosity << " +/- "
                << run.run_data.visc_err << Oil_run::visc_units();
    }

    Oil_run &operator>>(std::istream &in, oil::Oil_run &run) {
        in.read((char *) &run.run_data, sizeof(Oil_run::data));
        return run;
    }

    Oil_run &operator>>(const Oil_run::data &runData, Oil_run &run) {
        strcpy(run.run_data.name, runData.name);
        run.run_data.a = runData.a; run.run_data.a_err = runData.a_err;
        run.run_data.b = runData.b; run.run_data.b_err = runData.b_err;
        run.run_data.drum = runData.drum; run.run_data.drum_err = runData.drum_err;
        run.run_data.MT_v_l_slope = runData.MT_v_l_slope; run.run_data.MT_v_l_slope_err = runData.MT_v_l_slope_err;
        run.run_data.intercept = runData.intercept; run.run_data.intercept_err = runData.intercept_err;
        run.run_data.k = runData.k; run.run_data.k_err = runData.k_err;
        run.run_data.mass = runData.mass; run.run_data.mass_err = runData.mass_err;
        run.run_data.T = runData.T; run.run_data.T_err = runData.T_err;
        run.run_data.submergence = runData.submergence; run.run_data.sub_err = runData.sub_err;
        run.run_data.viscosity = runData.viscosity; run.run_data.visc_err = runData.visc_err;
        return run;
    }

    void string_upper(std::string &str) {
        for (char &ch : str) {
            ch = (char) std::toupper(ch);
        }
    }

    void string_upper(char *str) {
        if (str == nullptr) {
            return;
        }
        while (*str) {
            *str = (char) std::toupper(*str);
            ++str;
        }
    }

    bool is_numeric(const char *str) {
        if (str == nullptr) {
            return false;
        }
        while (*str) {
            if (!isdigit(*str++)) {
                return false;
            }
        }
        return true;
    }

    bool is_numeric(const std::string &str) {
        if (str.empty()) {
            return false;
        }
        for (const char &ch : str) {
            if (!isdigit(ch)) {
                return false;
            }
        }
        return true;
    }

    void clear_cin() {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    template <typename PATH>
    PATH get_home_path() {
        return {};
    }

    template <> char *get_home_path<char *>() {
#ifdef _WIN32
        char *home_path_c = (char *) malloc(MAX_PATH);
        if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, home_path_c) != S_OK) {
            return nullptr;
        }
        home_path_c = (char *) realloc(home_path_c, strlen_c(home_path_c) + 1);
        return home_path_c;
#else
        struct passwd *pwd;
        uid_t uid = getuid();
        pwd = getpwuid(uid);
        char *retval;
        retval = (char *) malloc(sizeof(char)*(strlen(pwd->pw_dir) + 1));
        strcpy(retval, pwd->pw_dir);
        return retval;
#endif
    }

    template <> std::string get_home_path<std::string>() {
#ifdef _WIN32
        char home_path_c[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, home_path_c) != S_OK) {
            return {};
        }
        return {home_path_c};
#else
        struct passwd *pwd;
        uid_t uid = getuid();
        pwd = getpwuid(uid);
        return {pwd->pw_dir};
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
        if (S_ISDIR(test.st_mode)) {
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
