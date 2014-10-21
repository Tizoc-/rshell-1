#include <iostream>
#include <unistd.h>
#include <string>
#include <string.h>
#include <memory>
#include <vector>
#include <list>
#include <sstream>
#include <map>
#include <algorithm>
#include <stdio.h>
#include <boost/tokenizer.hpp>
#include <sys/types.h>
#include <sys/wait.h>

const std::multimap<std::string, int>
DEFINED_OPS = {
    std::make_pair("&",2),
    std::make_pair("|",2),
    std::make_pair(";",1)
};

const std::vector<std::string> IMPLEMENTED_OPS{"&&","||",";"};
const std::vector<std::string> DEFINED_ADJACENT_OPS = {};


void Execute(std::list<std::string>& input);
bool UseOperator(std::list<std::string>& input, bool prevcommandstate);

void DumpCommand(std::list<std::string>& input);
bool UseCommand(std::list<std::string>& input);
bool ContainsImplementedOp(std::string);


void RepeatSweep(const std::list<std::string>& input); 
bool InvalidRepeatOp(const std::list<std::string>& input, std::pair<std::string,int> op); 
void CombineDefinedOps(std::list<std::string>& input); 
bool UnimplementedOp(const std::list<std::string>& input, std::string op); 

//uses unix calls to get username and hostname
//returns 'shellish' formatted str ie usrname@host:~$ 
std::string UserHostInfo();

//output username@hostname$ and wait for command input
std::string Prompt();

//separates string by delimeters into vector elements
//if # is found it excludes it and anything beyond in the string
std::list<std::string> Split(const std::string& input);


void Output(std::list<std::string>& input);

// takes the input list and an operator character and merges all repeating instances of that character within the list
// operators in shell can use the same symbol a different amount of times to represent different things
// ie bitwise & vs and &&
// the delimiter method I used to separate the arguments in the first place only had single character delimiting available
// since the function is general it will:
// avoid having to create new handcrafted parses when more features have to be added
// make bug checking general and simple (is & implemented? is &&& implemented? if not theyre bugs)
void RebuildOps(std::list<std::string>& input, std::string op);

int main() {
    while(true) {
        auto cmd = Prompt();
        auto input = Split(cmd);
        CombineDefinedOps(input);
        RepeatSweep(input);
        Execute(input);
    }
}
std::string UserHostInfo() {
    std::string loginname(getlogin());

    char *rawhost= new char[100];
    gethostname(rawhost,100);
    std::string hostname(rawhost);
    delete [] rawhost;

    std::string pwd(get_current_dir_name());

    //handles /home/username -> ~/ shortcut
    std::string target = "/home/"+loginname+"/";
    if (pwd.find(target) == 0) {
        pwd.erase(0,target.size());
        pwd = "~/"+pwd;
    }
    return loginname+"@"+hostname+":"+pwd+"$ ";
}
std::string Prompt() {
    std::cout << "->" << UserHostInfo();
    std::string input;
    std::getline(std::cin, input);
    return input;
}
std::list<std::string> Split(const std::string& input) {
    using namespace boost;
    using namespace std;
    list<string> input_split;
    char_separator<char> sep(" ", "#&|;");   
    typedef tokenizer<char_separator<char>> tokener;

    tokener tokens(input, sep);

    //complexity increases if I handle # as bash does.
    for (const auto& t : tokens) {
        if (t == "#") break;
        input_split.push_back(t);
    }
    return input_split;
}
void Output(std::list<std::string>& input) {
    using namespace std;
    for (const auto& e : input)
        cout << e << endl;

}
void RebuildOps(std::list<std::string>& input, std::string op) {
    using namespace std;
    auto front = input.begin();
    auto back = input.end();
    while (front != back) {
        auto element = find(front, back, op);
        int count = 0;
        while (element != back) {
            if (*element == op) {
                element = input.erase(element);
                count++;
            }
            else {
                break;
            }
        }
        std::string tempstr = "";
        while (count--) {
            tempstr += op;
        }
        if(!tempstr.empty())
            front = input.insert(element, tempstr);
        front++;
    }
}
void CombineDefinedOps(std::list<std::string>& input) {
    for(const auto& op : DEFINED_OPS) {
        RebuildOps(input, op.first);
    }
}
void RepeatSweep(const std::list<std::string>& input) {
    using namespace std;
    for (const auto& op : DEFINED_OPS) {
        if(InvalidRepeatOp(input,op)) {
            cout << "Invalid '" << op.first << "' usage found" << endl
                 << "known operator used an invalid amount of consecutive" << endl
                 << "times: e.g. '&&&' -> '&&' ?" << endl;
        }
    }
}
bool InvalidRepeatOp(const std::list<std::string>& input, std::pair<std::string,int> pair) {
    std::string rebuilt_op = "";
    auto op_size = pair.second;
    while(op_size--) {
        rebuilt_op+= pair.first;
    }
    auto front = input.begin();
    auto back = input.end();
    while(front != back) {
        auto itr = std::find_if(front, back,
                [&](std::string elem) {
                if (elem.find(rebuilt_op) == std::string::npos)
                    return false;
                else
                    return true;
                });
        if (itr == back)
            return false;

        else if (*itr != rebuilt_op)
            break;

        else {
            front = itr;
            front++;
        }
    }
    return true;
}
bool UnimplementedOp(const std::list<std::string>& input, std::string op) {
    using namespace std;
    auto itr = find(input.begin(), input.end(), op);
    if(itr == input.end()) {
        cout << "operator '" << op << "' is unimplemented" << endl;
        return false;
    }
    return true;
}
bool UseCommand(std::list<std::string> &input) {
    using namespace std;

    //Take list of strings, make copies of their c_strs, and put into vector
    //a vector of char* can be used as char** if used as such
    vector<char *> vectorcommand;
    while (!input.empty() && !ContainsImplementedOp(input.front())) {
        string transferstr = input.front();
        input.pop_front();
        char* cstrcopy = new char[transferstr.size()+1];
        memcpy(cstrcopy, transferstr.c_str(), transferstr.size()+1);
        cstrcopy[transferstr.size()] = 0;
        vectorcommand.push_back(cstrcopy);
    }
    vectorcommand.push_back(NULL);

    char** rawcommand = &vectorcommand[0];
    pid_t wait_val;
    auto pid = fork();
    if (pid==-1) {
        perror("Error on fork:\n");
        exit(1);
    }
    if (pid==0) {
        execvp(rawcommand[0], rawcommand);
        if(errno!=0) {
            perror("Error in execvp. Likely a nonexisting command?:\n");
        }
        for (size_t i = 0; i < vectorcommand.size(); i++)
            delete[] rawcommand[i];
    }
    else {
       wait_val = wait(0);
    }
    if (wait_val == -1) {
        perror("Error on waiting for child process to finish\n");
        exit(1);
    }
    return true;
}
bool ContainsImplementedOp(std::string token) {
    auto match = find(IMPLEMENTED_OPS.begin(), IMPLEMENTED_OPS.end(), token);
    if(match != IMPLEMENTED_OPS.end())
        return true;
    return false;
}
bool UseOperator(std::list<std::string>& input, bool prevcommandstate) {
    using namespace std;
    if(input.empty())
        return false;
    string op = input.front();
    input.pop_front();
    if (prevcommandstate == true) {
        if(op == ";")
            return true;
        else if(op == "&&")
            return true;
        else if(op == "||")
            return false;
    }
    else {
        if(op == ";")
            return true;
        else if(op == "&&")
            return false;
        else if(op == "||")
            return true;
    }
    //proper input ensures we never get down here, so im killing warning message
    //fixing this 'properly' would make it annoying to add more operators later
    return false;
}
void Execute(std::list<std::string>& input) {
    bool cmdstate = true;
    while(!input.empty()) {
        if (cmdstate)
            cmdstate = UseCommand(input);
        else 
            DumpCommand(input);
        cmdstate = UseOperator(input, cmdstate);
    }
}
void DumpCommand(std::list<std::string>& input) {
    while (!input.empty() && !ContainsImplementedOp(input.front())) {
        input.pop_front();
    }
}
