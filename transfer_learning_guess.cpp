//////////////////////////////////////////////////////////////////////////////////////////////////////////
//   pcfg_manager - creates password guesses using a probablistic context free grammar
//
//   Copyright (C) Matt Weir <weir@cs.fsu.edu>
//
//

#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <deque>
#include <list>
#include <queue>

//using namespace std;

#define MAXWORDSIZE 20 //Maximum size of a word from the input dictionaries
#define MAXINPUTDIC 1  //Maximum number of user inputed dictionaries

#ifdef _WIN32
#define PATH_DELIMITER '\\'
#else
#define PATH_DELIMITER '/'
#endif

//declare variables in config file
std::string model_path, guesses_file;
long guess_number, password_max_len, password_min_len;


///////////////////////////////////////////
//Used for initially parsing the dictionary words
typedef struct {
    short word_size{};
    int category{};
    double probability{};
    std::string word;
} dic_holder_t;


///////////////////////////////////////////
//Non-Terminal Container Struct
//Holds all the base information used for non-terminal to terminal replacements
typedef struct ntContainerStruct {
    double probability{};    //the probability of this group
    std::list<std::string> word;           //the replacement value, can be a dictionary word, a
    ntContainerStruct *next{};        //The next highest probable replacement for this type
} ntContainerType;

//////////////////////////////////////////
//PriorityQueue Replacement Type
//Basically a pointer
typedef struct pqReplacementStruct {
    int pivotPoint{};
    double probability{};
    double base_probability{};  //the probability of the base structure
    std::deque<ntContainerType *> replacement;
} pqReplacementType;

std::ofstream output_password;

unsigned long long count = 0;


class queueOrder {
public:
    queueOrder() = default;

    bool operator()(const pqReplacementType &lhs, const pqReplacementType &rhs) const {
        return (lhs.probability < rhs.probability);
    }
};

typedef std::priority_queue<pqReplacementType, std::vector<pqReplacementType>, queueOrder> pqueueType;

bool processBasicStruct(pqueueType *pQueue, ntContainerType **dicWords, ntContainerType **numWords,
                        ntContainerType **specialWords);

bool generateGuesses(pqueueType *pQueue);

int createTerminal(pqReplacementType *curQueueItem, int workingSection, std::string *curOutput, double prob);

bool pushNewValues(pqueueType *pQueue, pqReplacementType *curQueueItem);

void help();  //prints out the usage info

//Process the input Dictionaries
bool processDic(std::string *inputDicFileName, const double *inputDicProb, ntContainerType **dicWords);

bool processProbFromFile(ntContainerType **mainContainer, char *fileType);  //processes the number probabilities
//used to find the length of a possible non-ascii string, used because MACOSX had problems with wstring
short findSize(std::string input);


int main(int argc, char *argv[]) {
    std::string inputDicFileName[MAXINPUTDIC];

    double inputDicProb[MAXINPUTDIC];

    ntContainerType *dicWords[MAXWORDSIZE];
    ntContainerType *numWords[MAXWORDSIZE];
    ntContainerType *specialWords[MAXWORDSIZE];

    pqueueType pqueue;
//---------Parse the command line------------------------//

    //--Initilize the command line variables -- //
    for (double &i : inputDicProb) {
        i = 1;
    }

    if (argc == 1) {
        help();
    }
    std::string _help = "--help";
    std::string _trained_model = "--trained-model";
    std::string _guesses_file = "--guesses-file";
    std::string _guess_number = "--guess-number";
    std::string _guess_min_len = "--guess-min-len";
    std::string _guess_max_len = "--guess-max-len";
    std::string _verbose = "--with-prob";
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], _help.c_str(), _help.length()) == 0) {
            help();
            return 0;
        } else if (strncmp(argv[i], _trained_model.c_str(), _trained_model.length()) == 0) {
            i += 1;
            model_path = argv[i];
            if (model_path[model_path.size() - 1] != PATH_DELIMITER) {
                model_path += PATH_DELIMITER;
            }
        } else if (strncmp(argv[i], _guesses_file.c_str(), _guesses_file.length()) == 0) {
            i += 1;
            guesses_file = argv[i];
        } else if (strncmp(argv[i], _guess_number.c_str(), _guess_number.length()) == 0) {
            i += 1;
            guess_number = strtol(argv[i], nullptr, 0);
        } else if (strncmp(argv[i], _guess_min_len.c_str(), _guess_min_len.length()) == 0) {
            i += 1;
            password_min_len = strtol(argv[i], nullptr, 0);
        } else if (strncmp(argv[i], _guess_max_len.c_str(), _guess_max_len.length()) == 0) {
            i += 1;
            password_max_len = strtol(argv[i], nullptr, 0);
        }

    }
    std::string line;

    if (password_min_len > password_max_len) {
        std::cerr << "Error: min length cannot larger than max length!" << std::endl;
        return -1;
    }

    //---------Process all the Dictioanry Words------------------//
    if (model_path.empty()) {
        std::cout << "Need trained model" << std::endl;
        std::exit(-1);
    }
    inputDicFileName[0] = model_path + "dictionary.txt";
    if (!processDic(inputDicFileName, inputDicProb, dicWords)) {
        std::cerr << "\nThere was a problem opening the input dictionaries\n";
        help();
        return 0;
    }

#ifdef _WIN32
    if (processProbFromFile(numWords,"model\\digits\\")==false) {
#else
    if (!processProbFromFile(numWords, (char *) "model/digits/")) {
#endif
        std::cerr << "\nCould not open the number probability files\n";
        return 0;
    }
#ifdef _WIN32
    if (processProbFromFile(specialWords,"model\\special\\")==false) {
#else
    if (!processProbFromFile(specialWords, (char *) "model/special/")) {
#endif
        std::cerr << "\nCould not open the special character probability files\n";
        return 0;
    }
    if (!processBasicStruct(&pqueue, dicWords, numWords, specialWords)) {
        std::cerr << "\nError, could not open structure file from the training set\n";
        return 0;
    }

    output_password.open(guesses_file.c_str());
    if (!generateGuesses(&pqueue)) {
        std::cerr << "\nError generating guesses\n";
        return 0;
    }

    return 0;
}


void help() {
    std::cout << "Usage Info:\n"
                 "--guesses-file\tpwd generated will be placed here\n"
                 "--guess-number\tnumber of pwd to be generated\n"
                 "--guess-min-len\tpwd with length shorter than this will be ignored\n"
                 "--guess-max-len\tpwd with length longer than this will be ignored" << std::endl;
    std::exit(-1);
}

bool compareDicWords(const dic_holder_t &first, const dic_holder_t &second) {
    int compareValue = first.word.compare(second.word);
    if (compareValue < 0) {
        return true;
    } else if (compareValue > 0) {
        return false;
    } else if (first.probability > second.probability) {
        return true;
    }
    return false;
}

bool duplicateDicWords(const dic_holder_t &first, const dic_holder_t &second) {
    return first.word == second.word;
}

bool processDic(std::string *inputDicFileName, const double *inputDicProb, ntContainerType **dicWords) {
    std::ifstream inputFile;
    bool atLeastOneDic = false;  //just checks to make sure at least one input dictionary was specified
    dic_holder_t tempWord;
    std::list<dic_holder_t> allTheWords;
    size_t curPos;
    int numWords[MAXINPUTDIC][MAXWORDSIZE];
    double wordProb[MAXINPUTDIC][MAXWORDSIZE];
    ntContainerType *tempContainer;
    ntContainerType *curContainer;

    for (auto &numWord : numWords) {
        for (int &j : numWord) {
            j = 0;
        }
    }

    for (int i = 0; i < MAXINPUTDIC; i++) {  //for every input dictionary

        inputFile.open(inputDicFileName[i].c_str());
        if (!inputFile.is_open()) {
            std::cerr << "Could not open file " << inputDicFileName[i] << std::endl;
            return false;
        }
        tempWord.category = i;
        while (!inputFile.eof()) {
            std::getline(inputFile, tempWord.word);
            curPos = tempWord.word.find('\r');  //remove carrige returns
            if (curPos != std::string::npos) {
                tempWord.word.resize(curPos);
            }
            tempWord.word_size = findSize(tempWord.word);
            if ((tempWord.word_size > 0) && (tempWord.word_size < MAXWORDSIZE)) {


                allTheWords.push_front(tempWord);
                numWords[i][tempWord.word_size]++;

            }
        }
        atLeastOneDic = true;
        inputFile.close();

    }
    if (!atLeastOneDic) {
        return false;
    }
    //--Calculate probabilities --//
    for (int i = 0; i < MAXINPUTDIC; i++) {
        for (int j = 0; j < MAXWORDSIZE; j++) {
            if (numWords[i][j] == 0) {
                wordProb[i][j] = 0;
            } else {
                wordProb[i][j] = inputDicProb[i] * (1.0 / numWords[i][j]);
            }
        }
    }
    std::list<dic_holder_t>::iterator it;
    for (it = allTheWords.begin(); it != allTheWords.end(); ++it) {
        (*it).probability = wordProb[(*it).category][(*it).word_size];
    }

    allTheWords.sort(compareDicWords);
    allTheWords.unique(duplicateDicWords);

    //------Now divide the words into their own ntStructures-------//
    for (int i = 0; i < MAXWORDSIZE; i++) {
        dicWords[i] = nullptr;
        for (auto &j : wordProb) {
            if (j[i] != 0) {
                tempContainer = new ntContainerType;
                tempContainer->next = nullptr;
                tempContainer->probability = j[i];
                if (dicWords[i] == nullptr) {
                    dicWords[i] = tempContainer;
                } else if (dicWords[i]->probability < tempContainer->probability) {
                    tempContainer->next = dicWords[i];
                    dicWords[i] = tempContainer;
                } else {
                    curContainer = dicWords[i];
                    while ((curContainer->next != nullptr) &&
                           (curContainer->next->probability > tempContainer->probability)) {
                        curContainer = curContainer->next;
                    }
                    tempContainer->next = curContainer->next;
                    curContainer->next = tempContainer;
                }
            }
        }
    }
    for (it = allTheWords.begin(); it != allTheWords.end(); ++it) {
        tempContainer = dicWords[(*it).word_size];
        while ((tempContainer != nullptr) && (tempContainer->probability != (*it).probability)) {
            tempContainer = tempContainer->next;
        }
        if (tempContainer == nullptr) {
            std::cerr << "Error with processing input dictionary\n";
            std::cerr << "Word " << (*it).word << " prob " << (*it).probability << std::endl;
            return false;
        }
        tempContainer->word.push_back((*it).word);
    }


    return true;
}

short findSize(
        std::string input) { //used to find the size of a string that may contain wchar info, not using wstrings since MACOSX is being stupid
    //aka when I built it on Ubuntu it worked with wstring, but mac still reads them in as 8 bit chars
    short size = 0;
    for (int i = (int) input.size() - 1; i >= 0; i--) {
        if ((unsigned int) input[i] > 127) { //it is a non-ascii char
            i--;
        }
        size++;
    }
    return size;
}


bool processProbFromFile(ntContainerType **mainContainer, char *type) {  //processes the number probabilities
    bool atLeastOneValue = false;
    std::ifstream inputFile;
    std::list<std::string>::iterator it;
    char fileName[256];
    ntContainerType *curContainer;
    std::string inputLine;
    size_t marker;
    double prob;

    for (int i = 0; i < MAXWORDSIZE; i++) {
        sprintf(fileName, "%s%i.txt", type, i);
        std::string filename = model_path + fileName;
        inputFile.open(filename.c_str());
        if (inputFile.is_open()) { //a file exists for that string length
            curContainer = new ntContainerType;
            curContainer->next = nullptr;
            mainContainer[i] = curContainer;
            mainContainer[i]->probability = 0;
            while (!inputFile.eof()) {
                getline(inputFile, inputLine);
                marker = inputLine.find('\t');
                if (marker != std::string::npos) {
                    prob = strtod(inputLine.substr(marker + 1, inputLine.size()).c_str(), nullptr);
                    if ((curContainer->probability == 0) || (curContainer->probability == prob)) {
                        curContainer->probability = prob;
                        curContainer->word.push_back(inputLine.substr(0, marker));
                    } else {
                        curContainer->next = new ntContainerType;
                        curContainer = curContainer->next;
                        curContainer->next = nullptr;
                        curContainer->probability = prob;
                        curContainer->word.push_back(inputLine.substr(0, marker));
                    }
                }
            }
            atLeastOneValue = true;
            inputFile.close();
        } else {
            mainContainer[i] = nullptr;
        }
    }
    if (!atLeastOneValue) {
        std::cerr << "Error trying to open the probability values from the training set\n";
        return false;
    }
    return true;
}


bool processBasicStruct(pqueueType *pQueue, ntContainerType **dicWords, ntContainerType **numWords,
                        ntContainerType **specialWords) {
    std::ifstream inputFile;
    std::string inputLine;
    size_t marker;
    double prob;
    pqReplacementType inputValue;
    char pastCase;
    int curSize = 0;
    bool badInput;
#ifdef _WIN32
    std::string file = ".\\" + model_path + "model\\grammar\\structures.txt";
      inputFile.open(file.c_str());
#else
    std::string file = model_path + "model/grammar/structures.txt";
    inputFile.open(file.c_str());
#endif

    if (!inputFile.is_open()) {
        std::cerr << "Could not open the grammar file" << file << std::endl;
        return false;
    }
    inputValue.pivotPoint = 0;
    while (!inputFile.eof()) {
        badInput = false;
        getline(inputFile, inputLine);
        marker = inputLine.find('\t');
        if (marker != std::string::npos) {
            prob = strtod(inputLine.substr(marker + 1, inputLine.size()).c_str(), nullptr);
            inputLine.resize(marker);
            inputValue.probability = prob;
            inputValue.base_probability = prob;
            pastCase = '!';
            curSize = 0;
            for (char i : inputLine) {
                if (curSize == MAXWORDSIZE) {
                    badInput = true;
                    break;
                }
                if (pastCase == '!') {
                    pastCase = i;
                    curSize = 1;
                } else if (pastCase == i) {
                    curSize++;
                } else {
                    if (pastCase == 'L') {
                        if (dicWords[curSize] == nullptr) {
                            badInput = true;
                            break;
                        }
                        inputValue.replacement.push_back(dicWords[curSize]);
                        inputValue.probability = inputValue.probability * dicWords[curSize]->probability;
                    } else if (pastCase == 'D') {
                        if (numWords[curSize] == nullptr) {
                            badInput = true;
                            break;
                        }
                        inputValue.replacement.push_back(numWords[curSize]);
                        inputValue.probability = inputValue.probability * numWords[curSize]->probability;
                    } else if (pastCase == 'S') {
                        if (specialWords[curSize] == nullptr) {
                            badInput = true;
                            break;
                        }
                        inputValue.replacement.push_back(specialWords[curSize]);
                        inputValue.probability = inputValue.probability * specialWords[curSize]->probability;
                    } else {
                        std::cerr << "WTF Weird Error Occurred\n";
                        return false;
                    }
                    curSize = 1;
                    pastCase = i;
                }
            }
            if (badInput) { //NOOP
            } else if (pastCase == 'L') {
                if ((curSize >= MAXWORDSIZE) || (dicWords[curSize] == nullptr)) {
                    badInput = true;
                } else {
                    inputValue.replacement.push_back(dicWords[curSize]);
                    inputValue.probability = inputValue.probability * dicWords[curSize]->probability;
                }
            } else if (pastCase == 'D') {
                if ((curSize >= MAXWORDSIZE) || (numWords[curSize] == nullptr)) {
                    badInput = true;
                } else {
                    inputValue.replacement.push_back(numWords[curSize]);
                    inputValue.probability = inputValue.probability * numWords[curSize]->probability;
                }
            } else if (pastCase == 'S') {
                if ((curSize >= MAXWORDSIZE) || (specialWords[curSize] == nullptr)) {
                    badInput = true;
                } else {
                    inputValue.replacement.push_back(specialWords[curSize]);
                    inputValue.probability = inputValue.probability * specialWords[curSize]->probability;
                }
            }
            if (!badInput) {
                if (inputValue.probability == 0) {
                    std::cerr << "Error, we are getting some values with 0 probability\n";
                    return false;
                }
                pQueue->push(inputValue);
            }


            inputValue.probability = 0;
            inputValue.replacement.clear();
        }
    }

    inputFile.close();
    return true;
}


bool generateGuesses(pqueueType *pQueue) {
    pqReplacementType curQueueItem;
    int returnStatus;
    std::string curGuess;
    while (!pQueue->empty()) {
        curQueueItem = pQueue->top();
        pQueue->pop();
        curGuess.clear();
        returnStatus = createTerminal(&curQueueItem, 0, &curGuess, curQueueItem.base_probability);
        if (returnStatus == 1) { //made the maximum number of guesses
            return true;
        } else if (returnStatus == -1) { //an error occured
            return false;
        }
        pushNewValues(pQueue, &curQueueItem);
    }
    return true;
}


int createTerminal(pqReplacementType *curQueueItem, int workingSection, std::string *curOutput, double curProb) {
    std::list<std::string>::iterator it;
    int size = curOutput->size();
    curProb *= curQueueItem->replacement[workingSection]->probability;
    for (it = curQueueItem->replacement[workingSection]->word.begin();
         it != curQueueItem->replacement[workingSection]->word.end(); ++it) {
        curOutput->resize(size);
        curOutput->append(*it);
        if (workingSection == curQueueItem->replacement.size() - 1) {
            if ((curOutput->size() >= password_min_len) &&
                (curOutput->size() <= password_max_len)) {
                count++;
                if (!guesses_file.empty() && count <= guess_number) {
                    output_password << *curOutput << '\n';
                } else {
                    output_password.flush();
                    output_password.close();
                    std::exit(0);
                }
            } else {
                return 0;
            }


        } else {
            createTerminal(curQueueItem, workingSection + 1, curOutput, curProb);
        }
    }

    return 0;
}

bool pushNewValues(pqueueType *pQueue, pqReplacementType *curQueueItem) {
    pqReplacementType insertValue;

    insertValue.base_probability = curQueueItem->base_probability;
    for (int i = curQueueItem->pivotPoint; (unsigned long) i < curQueueItem->replacement.size(); i++) {
        if (curQueueItem->replacement[i]->next != nullptr) {
            insertValue.pivotPoint = i;
            insertValue.replacement.clear();
            insertValue.probability = curQueueItem->base_probability;
            for (int j = 0; (unsigned long) j < curQueueItem->replacement.size(); j++) {
                if (j != i) {
                    insertValue.replacement.push_back(curQueueItem->replacement[j]);
                    insertValue.probability = insertValue.probability * curQueueItem->replacement[j]->probability;
                } else {
                    insertValue.replacement.push_back(curQueueItem->replacement[j]->next);
                    insertValue.probability = insertValue.probability * curQueueItem->replacement[j]->next->probability;
                }
            }
            pQueue->push(insertValue);
        }
    }
    return true;
}
