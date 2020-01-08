#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <deque>
#include <queue>
#include <map>
#include <algorithm>
#include <sys/stat.h>
#include <utility>
#include <dirent.h>
#include "include/clipp.h"


#ifdef _WIN32
#define PATH_DELIMITER '\\'
#define NEW_LINE "\r\n"
#else
#define PATH_DELIMITER '/'
#define NEW_LINE "\n"
#endif


class Entry {
public:;

    Entry() {
        str = "";
        cnt = 0;
    }

    std::string getStr() {
        return str;
    }

    int getCnt() {
        return cnt;
    }

protected:
    std::string str;
    int cnt;
};


/**
 * keep the structure of the passwords, and the frequency of the structure.
 * for example, 
 * Structure *s = new Structure("LLL", 4);
 */
class Structure : public Entry {
public:
    Structure(std::string data, int count) {
        str = std::move(data);
        cnt = count;
    };
};

/**
 * keep the digit part of the passwords, and the frequency of the digit.
 * for example, 
 * Digit *s = new Digit("1234", 0.4);
 */
class Digit {
public:
    Digit(std::string data, float p) {
        str = std::move(data);
        prob = p;
    };

    std::string getStr() {
        return str;
    }

    float getProb() {
        return prob;
    }

protected:
    std::string str;
    float prob;
};

/**
 * keep the special part of the passwords, and the frequency of the special.
 * for example, 
 * Special *s = new Special("^_^", 0.4);
 */
class Special {
public:
    Special(std::string data, float p) {
        str = std::move(data);
        prob = p;
    };

    std::string getStr() {
        return str;
    }

    float getProb() {
        return prob;
    }

protected:
    std::string str;
    float prob;
};


std::string model_output_path;
std::string tmp_model_output_path;
std::string external_dict_path;
int transfer_min_len = 1;
int transfer_max_len = 255;

int training_set_size;
int useful_set_size;
int dictionary_size;

std::map<std::string, int> structure_map;
std::map<std::string, int> digit_map_long;
std::map<std::string, int> digit_map_short;
std::map<std::string, int> letter_map_long;
std::map<std::string, int> letter_map_short;
std::map<std::string, int> special_map_long;
std::map<std::string, int> special_map_short;


/**
 * definition
 */
void help();


void extract_structure(const char *line, int size);

void extract_digit(const char *line, int size, unsigned int min_len, std::map<std::string, int> &digit_map);

void extract_letter(const char *line, int size, unsigned int min_len, std::map<std::string, int> &letter_map);

void extract_special(const char *line, int size, unsigned int min_len, std::map<std::string, int> &special_map);

bool negative_sort_structure(Structure *e1, Structure *e2);

void process_structure();

void process_digit();

void process_special();

void process_letter();

void create_dir(const char *dir);

float calc_weight(int size);

int get_training_set_size(const char *training_set);

int rm_dir(const std::string &dir_full_path);

int main(int argc, char *argv[]) {
    std::string training_set;
    std::vector<std::string> vec;
    bool rm_existed = false;
    int start_from = 8;
    // parse arguments
    auto cli_trained_model = clipp::in_sequence(
            clipp::required("--trained-model") &
            clipp::value("path to save the trained model", model_output_path).call([&]() {
                if (model_output_path[model_output_path.size() - 1] != PATH_DELIMITER)
                    model_output_path += PATH_DELIMITER;
                create_dir(model_output_path.c_str());
                tmp_model_output_path = model_output_path + "model" + PATH_DELIMITER;
                create_dir(tmp_model_output_path.c_str());
            }),
            clipp::option("--rm-existed").set(rm_existed).call([&]() {
                std::cerr << "Remove existed model" << std::endl;
                std::string digit_folder = tmp_model_output_path + PATH_DELIMITER + "digits";
                std::string special_folder = tmp_model_output_path + PATH_DELIMITER + "special";
                std::string struct_folder = tmp_model_output_path + PATH_DELIMITER + "grammar";
                rm_dir(digit_folder);
                rm_dir(special_folder);
                rm_dir(struct_folder);
            }).doc("remove model with same path, must be put after --trained-model flag"));
    auto cmd = (clipp::required("--training-set") & clipp::value("path of training set", training_set),
            cli_trained_model,
            clipp::option("--train-length-min") &
            clipp::value("min length to transfer", transfer_min_len).call([&]() {
                if (transfer_min_len < 0) {
                    std::cerr << "REPLACE " << transfer_min_len << " with " << 1 << std::endl;
                    transfer_min_len = 1;
                } else if (transfer_min_len > transfer_max_len) {
                    std::cerr << "MIN LEN larger than MAX LEN, set min len to " << transfer_max_len << std::endl;
                    transfer_min_len = transfer_max_len;
                }
            }),
            clipp::option("--train-length-max") &
            clipp::value("max length to transfer", transfer_max_len).call([&]() {
                if (transfer_max_len < transfer_min_len) {
                    std::cerr << "MIN LEN less than MAX LEN, set max len to " << transfer_min_len << std::endl;
                    transfer_max_len = transfer_min_len;
                }
            }),
            clipp::required("--dictionaries") &
            clipp::value("external dictionary, one item per line", external_dict_path),
            clipp::option("--start-from") & clipp::number("Combination start from", start_from).call([&start_from]() {
                std::cout << "start from: " << start_from << std::endl;
            }),
            clipp::option("-h", "--help") % "show help"
    );
    if (!clipp::parse(argc, argv, cmd)) {
        auto fmt = clipp::doc_formatting{}.doc_column(31);
        std::cerr << clipp::make_man_page(cmd, argv[0], fmt) << std::endl;
        std::exit(1);
    }
    std::string line;
    if (transfer_min_len > transfer_max_len) {
        std::cerr << "Error: min length larger than max length!" << std::endl;
        return -1;
    }
    training_set_size = get_training_set_size(training_set.c_str());
    if (training_set_size == -2) {
        return -1;
    }
    std::ifstream input_training;
    input_training.open(training_set.c_str());
    if (!input_training.is_open()) {
        std::cerr << "Could not open file " << training_set << std::endl;
        return -1;
    }

    /**
     * training
     */
    while (!input_training.eof()) {
        getline(input_training, line);
        int size = line.size();
        if (size <= 0) {
            continue;
        }
        if (transfer_min_len <= size && size <= transfer_max_len) {
            useful_set_size += 1;
            extract_structure(line.c_str(), size);
            extract_digit(line.c_str(), size, 1, digit_map_long);
            extract_letter(line.c_str(), size, 1, letter_map_long);
            extract_special(line.c_str(), size, 1, special_map_long);
        } else if (size >= start_from && size < transfer_min_len) {
            extract_digit(line.c_str(), size, size, digit_map_short);
            extract_letter(line.c_str(), size, size, letter_map_short);
            extract_special(line.c_str(), size, size, special_map_short);
        } else if (0 < size && size < start_from) {
            extract_digit(line.c_str(), size, 1, digit_map_short);
            extract_letter(line.c_str(), size, 1, letter_map_short);
            extract_special(line.c_str(), size, 1, special_map_short);
        }
    }
    process_structure();
    process_digit();
    process_special();
    process_letter();
    return 0;

}

/**
 * how to use
 */
void help() {
    std::cout << "Usage Info:\n";
    std::cout << "--training-set\t\ttraining set\n"
                 "--trained-model\t\ttrained model will be placed here\n"
                 "--train-length-min\tpwd with length less than this value will be ignored\n"
                 "--train-length-max\tpwd wilt length longer than this value will be ignored\n"
                 "--dictionaries\t\tto enrich the grammar of letter";
    std::cout << std::endl;
    std::exit(0);
}

// extract structure info
void extract_structure(const char *line, int size) {
    std::string result;
    for (int i = 0; i < size; i++) {
        if ('0' <= line[i] && '9' >= line[i]) {
            result += "D";
        } else if (('a' <= line[i] && 'z' >= line[i]) || ('A' <= line[i] && 'Z' >= line[i])) {
            result += "L";
        } else if (0 <= (int) line[i] && 127 >= (int) line[i]) {
            result += "S";
        } else {
            break;
        }
    }
    if (structure_map.find(result) != structure_map.end()) {
        structure_map[result] += 1;
    } else {
        structure_map.insert(make_pair(result, 1));
    }

}

// extract digit part
void extract_digit(const char *line, int size, unsigned int min_len, std::map<std::string, int> &digit_map) {
    std::vector<std::string> d;
    std::string val;
    for (int i = 0; i < size; i++) {
        if (line[i] >= '0' && line[i] <= '9') {
            val += line[i];
        } else if (!val.empty()) {
            d.push_back(val);
            val = "";
        }
    }
    if (!val.empty()) {
        d.push_back(val);
    }
    int vec_size = d.size();
    for (int i = 0; i < vec_size; i++) {
        if (d[i].size() >= min_len) {
            if (digit_map.find(d[i]) != digit_map.end()) {
                digit_map[d[i]] += 1;
            } else {
                digit_map.insert(make_pair(d[i], 1));
            }
        }
    }
}

// extract letter part
void extract_letter(const char *line, int size, unsigned int min_len, std::map<std::string, int> &letter_map) {
    std::vector<std::string> l;
    std::string val;
    for (int i = 0; i < size; i++) {
        if ((line[i] >= 'a' && line[i] <= 'z') || (line[i] >= 'A' && line[i] <= 'Z')) {
            val += line[i];
        } else if (!val.empty()) {
            l.push_back(val);
            val = "";
        }
    }
    if (!val.empty()) {
        l.push_back(val);
    }
    int vec_size = l.size();
    for (int i = 0; i < vec_size; i++) {
        if (l[i].size() >= min_len) {
            if (letter_map.find(l[i]) != letter_map.end()) {
                letter_map[l[i]] += 1;
            } else {
                letter_map.insert(make_pair(l[i], 1));
            }
        }
    }
}

// extract special part
void extract_special(const char *line, int size, unsigned int min_len, std::map<std::string, int> &special_map) {
    std::vector<std::string> s;
    std::string val;
    for (int i = 0; i < size; i++) {
        if (!((line[i] >= 'a' && line[i] <= 'z')
              || (line[i] >= 'A' && line[i] <= 'Z')
              || (line[i] >= '0' && line[i] <= '9'))) {
            val += line[i];
        } else if (!val.empty()) {
            s.push_back(val);
            val = "";
        }
    }
    if (!val.empty()) {
        s.push_back(val);
    }
    int vec_size = s.size();
    for (int i = 0; i < vec_size; i++) {
        if (s[i].size() >= min_len) {
            if (special_map.find(s[i]) != special_map.end()) {
                special_map[s[i]] += 1;
            } else {
                special_map.insert(make_pair(s[i], 1));
            }
        }
    }
}

/**
 * sort the structure with negitive sequence
 */
bool negative_sort_structure(Structure *e1, Structure *e2) {
    return e1->getCnt() > e2->getCnt();
}

/**
 * write the structure and related probability to file
 */
void process_structure() {
    std::map<std::string, int>::iterator it;
    int total_structures_number = 0;
    std::vector<Structure *> structure_group;
    for (it = structure_map.begin(); it != structure_map.end(); it++) {
        auto *s = new Structure(it->first, it->second);
        structure_group.push_back(s);
        total_structures_number += it->second;
    }
    sort(structure_group.begin(), structure_group.end(), negative_sort_structure);
    int size = structure_group.size();

    std::string dir = tmp_model_output_path + "grammar";

    create_dir(dir.c_str());
    std::ofstream fout_structure((tmp_model_output_path + "grammar" + PATH_DELIMITER + "structures.txt").c_str());
    for (int i = 0; i < size; i++) {
        fout_structure << structure_group[i]->getStr() << '\x09' << std::fixed << std::setprecision(30)
                       << 1.0 * structure_group[i]->getCnt() / total_structures_number << std::endl;

    }
    fout_structure.close();
    for (auto &itr : structure_group) {
        delete itr;
    }
    structure_group.clear();
    structure_map.erase(structure_map.begin(), structure_map.end());
}

/**
 * sort the digits with negitive sequence
 */
bool negative_sort_digit(Digit *d1, Digit *d2) {
    return d1->getProb() > d2->getProb();
}

/**
 * transfer the probabilities of digits and write them down to the files
 */
void process_digit() {
    std::map<std::string, int>::iterator it;
    int arr_size = 256;
    int total_digit_long_number[arr_size];
    int total_digit_short_number[arr_size];
    for (int i = 0; i < arr_size; i++) {
        total_digit_long_number[i] = 0;
        total_digit_short_number[i] = 0;
    }
    for (it = digit_map_long.begin(); it != digit_map_long.end(); it++) {
        total_digit_long_number[it->first.size()] += it->second;
    }
    for (it = digit_map_short.begin(); it != digit_map_short.end(); it++) {
        total_digit_short_number[it->first.size()] += it->second;
    }
    float weight = calc_weight(useful_set_size);
    std::vector<Digit *> digit_group;

    for (it = digit_map_long.begin(); it != digit_map_long.end(); it++) {
        // both short and long
        if (digit_map_short.find(it->first) != digit_map_short.end()) {
            float prob_long = 1.0f * it->second / (float) total_digit_long_number[it->first.size()];
            float prob_short = 1.0f * digit_map_short[it->first] / (float) total_digit_short_number[it->first.size()];
            float prob_new = prob_long * weight + prob_short * (1 - weight);
            auto *d = new Digit(it->first, prob_new);
            digit_group.push_back(d);
        } else { // only long
            float prob_long = 1.0f * it->second / (float) total_digit_long_number[it->first.size()];
            float prob_new = prob_long * weight;
            auto *d = new Digit(it->first, prob_new);
            digit_group.push_back(d);
        }
    }
    // only short
    for (it = digit_map_short.begin(); it != digit_map_short.end(); it++) {
        if (digit_map_long.find(it->first) == digit_map_long.end()) {
            float prob_short = 1.0f * it->second / (float) total_digit_short_number[it->first.size()];
            float prob_new = prob_short * (1 - weight);
            auto *d = new Digit(it->first, prob_new);
            digit_group.push_back(d);
        }
    }
    sort(digit_group.begin(), digit_group.end(), negative_sort_digit);

    std::vector<Digit *> digit_groups[arr_size];
    int size = digit_group.size();
    std::string dir = tmp_model_output_path + "digits";
    create_dir(dir.c_str());
    for (int i = 0; i < size; i++) {

        int length = digit_group[i]->getStr().size();

        digit_groups[length].push_back(digit_group[i]);
    }
    for (int i = 0; i < arr_size; i++) {
        int cur_size = digit_groups[i].size();
        if (cur_size <= 0) {
            continue;
        }
        std::stringstream ss;
        ss << i;
        std::string number;
        ss >> number;
//        std::string digit_file = (tmp_model_output_path.append("digits").append(
//                reinterpret_cast<const char *>(PATH_DELIMITER)).append(number).append(".txt"));
        std::string digit_file = tmp_model_output_path;
        digit_file += "digits";
        digit_file += PATH_DELIMITER;
        digit_file += number;
        digit_file += ".txt";
        std::ofstream fout_i(digit_file.c_str());

        for (int j = 0; j < cur_size; j++) {
            fout_i << digit_groups[i][j]->getStr() << '\x09' << std::fixed << std::setprecision(30)
                   << digit_groups[i][j]->getProb() << std::endl;
            delete digit_groups[i][j];
        }
        digit_groups[i].clear();
    }

    digit_group.clear();
    digit_map_long.erase(digit_map_long.begin(), digit_map_long.end());
    digit_map_short.erase(digit_map_short.begin(), digit_map_short.end());
}

/**
 * sort the special with negative sequence
 */
bool negative_sort_special(Special *s1, Special *s2) {
    return s1->getProb() > s2->getProb();
}

/**
 * transfer the probabilities of special and write them down to the files
 */
void process_special() {
    std::map<std::string, int>::iterator it;
    int arr_size = 256;
    int total_special_long_number[arr_size];
    int total_special_short_number[arr_size];
    for (int i = 0; i < arr_size; i++) {
        total_special_long_number[i] = 0;
        total_special_short_number[i] = 0;
    }
    for (it = special_map_long.begin(); it != special_map_long.end(); it++) {
        total_special_long_number[it->first.size()] += it->second;
    }
    for (it = special_map_short.begin(); it != special_map_short.end(); it++) {
        total_special_short_number[it->first.size()] += it->second;
    }
    float weight = calc_weight(useful_set_size);
    std::vector<Special *> special_group;

    for (it = special_map_long.begin(); it != special_map_long.end(); it++) {
        // both short and long
        if (special_map_short.find(it->first) != special_map_short.end()) {
            float prob_long = 1.0f * it->second / (float) total_special_long_number[it->first.size()];
            float prob_short =
                    1.0f * special_map_short[it->first] / (float) total_special_short_number[it->first.size()];
            float prob_new = prob_long * weight + prob_short * (1 - weight);
            auto *d = new Special(it->first, prob_new);
            special_group.push_back(d);
        } else { // only long
            float prob_long = 1.0f * it->second / (float) total_special_long_number[it->first.size()];
            float prob_new = prob_long * weight;
            auto *d = new Special(it->first, prob_new);
            special_group.push_back(d);
        }
    }
    // only short
    for (it = special_map_short.begin(); it != special_map_short.end(); it++) {
        if (special_map_long.find(it->first) == special_map_long.end()) {
            float prob_short = 1.0f * it->second / (float) total_special_short_number[it->first.size()];
            float prob_new = prob_short * (1 - weight);
            auto *d = new Special(it->first, prob_new);
            special_group.push_back(d);
        }
    }
    sort(special_group.begin(), special_group.end(), negative_sort_special);

    std::vector<Special *> special_groups[arr_size];
    int size = special_group.size();
    std::string dir = tmp_model_output_path + "special";
    create_dir(dir.c_str());
    for (int i = 0; i < size; i++) {
        int length = special_group[i]->getStr().size();
        special_groups[length].push_back(special_group[i]);
    }
    for (int i = 0; i < arr_size; i++) {
        int cur_size = special_groups[i].size();
        if (cur_size <= 0) {
            continue;
        }
        std::stringstream ss;
        ss << i;
        std::string number;
        ss >> number;
//        std::string special_file = tmp_model_output_path.append("special").append(
//                reinterpret_cast<const char *>(PATH_DELIMITER)).append(number).append(".txt");
        std::string special_file = tmp_model_output_path;
        special_file += "special";
        special_file += PATH_DELIMITER;
        special_file += number;
        special_file += ".txt";
        std::ofstream fout_i(special_file.c_str());

        for (int j = 0; j < cur_size; j++) {
            fout_i << special_groups[i][j]->getStr() << '\x09' << std::fixed << std::setprecision(30)
                   << special_groups[i][j]->getProb() << std::endl;
            delete special_groups[i][j];
        }
        special_groups[i].clear();
    }

    special_group.clear();
    special_map_long.erase(special_map_long.begin(), special_map_long.end());
    special_map_short.erase(special_map_short.begin(), special_map_short.end());
}

/**
 * mix with dictionary and write them down to the files
 */
void process_letter() {
    std::map<std::string, int>::iterator it;
    for (it = letter_map_short.begin(); it != letter_map_short.end(); it++) {
        if (letter_map_long.find(it->first) == letter_map_long.end()) {
            letter_map_long.insert(make_pair(it->first, it->second));
        }
    }
    std::string letter_file = (model_output_path + "dictionary.txt");
    std::ofstream fout_letter(letter_file.c_str());
    for (it = letter_map_long.begin(); it != letter_map_long.end(); it++) {
        fout_letter << it->first << std::endl;
    }
    std::ifstream fin_dict(external_dict_path.c_str());
    if (fin_dict.is_open()) {
        std::string line;
        while (!fin_dict.eof()) {
            getline(fin_dict, line);
            if (letter_map_long.find(line) == letter_map_long.end()) {
                fout_letter << line << std::endl;
            }
        }
    }
    fin_dict.close();
    fout_letter.close();
    letter_map_long.erase(letter_map_long.begin(), letter_map_long.end());
    letter_map_short.erase(letter_map_short.begin(), letter_map_short.end());
}

/**
 * this function help us create a folder which does not exist
 */
void create_dir(const char *dir) {
    // create if dir does not exist. S_IRWXU|S_IRGRP|S_IROTH = 0744
    if (-1 == access(dir, 0)) {
#ifdef _WIN32
        if (-1 == mkdir(dir)) {
            cout << "[Error] create the path to put the model!" << endl;
        }
#else
        if (-1 == mkdir(dir, 0744)) {
            std::cerr << "[Error] create the path to put the model!" << std::endl;
        }
#endif
    }
}


/**
 * get the weight to be assigned to prob_long
 */
float calc_weight(int size) {
    int d = (int) ((1 / (1 + exp(10 - 2 * log10(size))) + 0.05) * 10);
    float w = (float) d / 10.0f;
    return w;
}


int get_training_set_size(const char *training_set) {
    std::ifstream input_training;
    input_training.open(training_set);
    int set_size = 0;
    if (!input_training.is_open()) {
        std::cerr << "[Training set]: Could not open file " << training_set << std::endl;
        return -1;
    }

    std::string line;
    while (!input_training.eof()) {
        getline(input_training, line);
        int size = line.size();
        if (size <= 0) {
            continue;
        }
        set_size += 1;
    }
    input_training.close();

    std::ifstream input_dict;
    input_dict.open(external_dict_path.c_str());
    if (!input_dict.is_open()) {
        std::cerr << "[Dict]: Could not open file " << external_dict_path << std::endl;
        std::cout << "will not use dictionary" << std::endl;
        dictionary_size = 0;
    } else {
        while (!input_dict.eof()) {
            getline(input_dict, line);
            dictionary_size += 1;
        }
    }
    input_dict.close();
    return set_size;
}

int rm_dir(const std::string &dir_full_path) {
    DIR *dirp = opendir(dir_full_path.c_str());
    if (!dirp) {
        return -1;
    }
    struct dirent *dir;
    struct stat st{};
    while ((dir = readdir(dirp)) != nullptr) {
        if (strcmp(dir->d_name, ".") == 0
            || strcmp(dir->d_name, "..") == 0) {
            continue;
        }
        std::string sub_path = dir_full_path + '/' + dir->d_name;
        if (lstat(sub_path.c_str(), &st) == -1) {
            //Log("rm_dir:lstat ",sub_path," error");
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (rm_dir(sub_path) == -1) // 如果是目录文件，递归删除
            {
                closedir(dirp);
                return -1;
            }
            rmdir(sub_path.c_str());
        } else if (S_ISREG(st.st_mode)) {
            unlink(sub_path.c_str());     // 如果是普通文件，则unlink
        } else {
            //Log("rm_dir:st_mode ",sub_path," error");
            continue;
        }
    }
    if (rmdir(dir_full_path.c_str()) == -1)//delete dir itself.
    {
        closedir(dirp);
        return -1;
    }
    closedir(dirp);
    return 0;
}
