// run a benchmark with limited number of retired instructions

// CSR_CYCLE 0xc00
// CSR_TIME 0xc01
// CSR_INSTRET 0xc02

#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define write_csr(reg, val) ({ \
  asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

#define read_cycle()   read_csr(0xc00)
#define read_time()    read_csr(0xc01)
#define read_instret() read_csr(0xc02)

#include <chrono>
#include <iostream>
#include <thread>
#include <cstdint>
#include <spawn.h>
#include <sys/wait.h>

#include "flexicas/flexicas-pfc.h"

class PFCRecord {
public:
  void start() { write_csr(0x8F0, FLEXICAS_PFC_CMD|FLEXICAS_PFC_START); }
  void end() { write_csr(0x8F0, FLEXICAS_PFC_CMD|FLEXICAS_PFC_STOP); }
  void prefix(const std::string &prefix) {
    write_csr(0x8F0, FLEXICAS_PFC_CMD|FLEXICAS_PFC_STR_CLR); // clear pfc string buffer
    for(auto c:prefix) write_csr(0x8F0, FLEXICAS_PFC_CMD|FLEXICAS_PFC_STR_CHAR|((0ull | c) << 16)); // send char
    write_csr(0x8F0, FLEXICAS_PFC_CMD|FLEXICAS_PFC_PREFIX);
  }
  uint64_t get_llc_acc() {
    write_csr(0x8F0, FLEXICAS_PFC_CMD|FLEXICAS_PFC_LLC_ACC); // clear pfc string bufferread llc acc
    uint64_t acc = read_csr(0x8F0);
    return acc;
  }
  
};

static PFCRecord pfc;

using namespace std::chrono_literals;

void run_cmd(char *argv[], char* envp[], uint64_t max_instret, const std::string &prefix) {
  pid_t pid;
  int rv = posix_spawnp(&pid, argv[0], NULL, NULL, argv, envp);

  if(rv) {
    if(rv == ENOSYS) {
      std::cerr << "posix_spawn() is NOT supported in this system!" << std::endl;
      exit(1);
    } else exit(rv);
  }

  if(!prefix.empty()) pfc.prefix(prefix);

  uint64_t instret_start = pfc.get_llc_acc();
  uint64_t instret_now = pfc.get_llc_acc();
  //std::cerr << "[timed-run] " << read_time() << ": successfully started with initial reading of " << instret_start << " instructions." << std::endl;
  pfc.start();
  int s, status;
  while(max_instret == 0 || instret_now - instret_start < max_instret) {
    s = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if(0 == s) { // child is running
      std::this_thread::sleep_for(100ms);
      instret_now = pfc.get_llc_acc();
      //std::cerr << "[timed-run] " << read_time() << ": successfully run " << instret_now - instret_start << " instructions." << std::endl;
    } else break;
  }

  pfc.end();

  if(s != 0) { // somthing is wrong
     if(s == -1)
       std::cerr << "waitpid() is NOT supported in this system!" << std::endl;
     else
       std::cerr << "process exit early!" << std::endl;
     exit(1);
  }

  kill(pid, SIGKILL);
  waitpid(pid, NULL, WNOHANG);
}

int main(int argc, char* argv[], char* envp[]) {
  unsigned int argi = 1;
  std::string arguement, prefix;

  while(true) {
    arguement = std::string(argv[argi++]);

    if(0 == arguement.compare(0,6,"--help")) {
      std::cout << "Usage: timed-run <arguement list> num program <program's argument list>" << std::endl;
      std::cout << "  argument list: --help          print this message." << std::endl;
      std::cout << "                 --log=prefix    set the prefix for log files." << std::endl;
      std::cout << "  num                            number of Million instructions to run," << std::endl;
      std::cout << "                                 0 means run until program finishes."<< std::endl;
      std::cout << "  program                        the program to run." << std::endl;
      std::cout << "  program's argument list        the arguments for the guest program." << std::endl;
      return 0;
    } else if(0 == arguement.compare(0,6,"--log=")) {
      prefix = arguement.substr(6);
    } else
      break;
  }

  uint64_t max_instret = 1000000ull * std::stoll(arguement);
  argv += argi;
  run_cmd(argv, envp, max_instret, prefix);
  return 0;
}
