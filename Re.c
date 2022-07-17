#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>

#ifndef errno
extern int errno;
#endif

#ifdef _WIN32
#include <shlobj.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

#define DAT_SIZE 192
#define NAME_LEN 32

inline int isdigit_c(char ch) {
    return ch >= 48 && ch <= 57 ? 1 : 0;
}

int is_numeric(const char *str) {
    if (str == NULL || *str == 0 || *str == '.') {
        return 0;
    }
    size_t point_count = 0;
    while (*str) {
        if (isdigit_c(*str) == 0 && *str != '.') {
            return 0;
        }
        if (*str++ == '.') {
            ++point_count;
        }
    }
    if (point_count > 1) {
        return 0;
    }
    return 1;
}

char *get_home_path() {
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

double *calc_Re(double rho, double rho_err, double a, double a_err, double b, double b_err, double eta, double eta_err,
               double T, double T_err) {
    double Re = (rho*M_PI*b*(a - b))/(2*eta*T);
    double a_minus_b_err = sqrt(a_err*a_err + b_err*b_err);
    double Re_err = Re*sqrt(pow(rho_err/rho, 2) + pow(b_err/b, 2) + pow(a_minus_b_err/(a - b), 2) + pow(eta_err/eta, 2)
                            + pow(T_err/T, 2));
    double *retval = (double *) malloc(2*sizeof(double));
    *retval = Re;
    *(retval + 1) = Re_err;
    return retval;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        errno = EINVAL;
        perror("Error");
        fprintf(stderr, "Invalid number of arguments provided.\n");
        return 1;
    }
    if (!is_numeric(*(argv + 1))) {
        errno = EINVAL;
        perror("Error");
        fprintf(stderr, "Density of the oil provided is not numeric.\n");
        return 1;
    }
    double rho = strtod(*(argv + 1), NULL);
    if (errno == ERANGE) {
        perror("Error");
        fprintf(stderr, "The inputted density could not be converted to a decimal number (a double).\n");
        return 1;
    }
    char *rest;
#ifdef _WIN32
    rest = "\\Exp_V_Program_Files\\All_Runs.dat";
#else
    rest = "/Exp_V_Program_Files/All_Runs.dat";
#endif
    char *dat_path = get_home_path();
    dat_path = (char *) realloc(dat_path, strlen(dat_path) + strlen(rest) + 1);
    strcat(dat_path, rest);
    struct stat buff;
    if (stat(dat_path, &buff) == -1 || S_ISDIR(buff.st_mode)) {
        errno = EIO;
        perror("An error occurred");
        fprintf(stderr, "Either the .dat file does not exist, is not in the right place, or is a directory.\n");
        return 1;
    }
    if (buff.st_size == 0) {
        errno = EINVAL;
        perror("An error occurred");
        fprintf(stderr, "The .dat file is empty.\n");
        return 1;
    }
    if (buff.st_size % DAT_SIZE) {
        perror("An error occurred");
        fprintf(stderr, "The .dat size does not contain an integer multiple of Oil_run structs.\n");
        return 1;
    }
    size_t num_structs = buff.st_size / DAT_SIZE;
    long int offset;
    char name[32];
    double a;
    double a_err;
    double b;
    double b_err;
    double T;
    double T_err;
    double visc;
    double v_err;
    double *Re = NULL;
    FILE *fp = fopen(dat_path, "r+");
    for (int i = 0; i < num_structs; ++i) {
        offset = i*DAT_SIZE;
        fseek(fp, offset, SEEK_SET);
        fread(name, 32, 1, fp);
        fread(&a, sizeof(double), 1, fp);
        fread(&a_err, sizeof(double), 1, fp);
        fread(&b, sizeof(double), 1, fp);
        fread(&b_err, sizeof(double), 1, fp);
        fseek(fp, 10*sizeof(double), SEEK_CUR);
        fread(&T, sizeof(double), 1, fp);
        fread(&T_err, sizeof(double), 1, fp);
        fseek(fp, 2*sizeof(double), SEEK_CUR);
        fread(&visc, sizeof(double), 1, fp);
        fread(&v_err, sizeof(double), 1, fp);
        Re = calc_Re(rho, 0, a, a_err, b, b_err, visc, v_err, T, T_err);
        printf("\nFor the Oil run with name: %s, Reynolds number is: %lf +/- %lf\n", name, *Re, *(Re + 1));
        free(Re);
    }
    printf("\n");
    fclose(fp);
    free(dat_path);
    return 0;
}
