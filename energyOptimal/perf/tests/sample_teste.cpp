#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>

#include <poll.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <memory.h>

using namespace std;

pid_t create_wrokload(char** argv)
{
    pid_t pid= fork();
    if(pid == 0)
    {
        int fd= open("output",  O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        dup2(fd, STDOUT_FILENO);
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        if( execl(argv[1], (const char *)argv+1, NULL) < 0)
        {
            cerr << "Error on executing worload" << endl;
        }
    }
    return pid;
}

int total=0;
static void perf_event_handler(int signum, siginfo_t* info, void* ucontext)
{
    if(info->si_code != POLL_HUP) {
        // Only POLL_HUP should happen.
        cerr << "Error in interruption" << endl;
        exit(EXIT_FAILURE);
    }
    static int64_t counter;
    read(info->si_fd, &counter, sizeof(int64_t));
    ioctl(info->si_fd, PERF_EVENT_IOC_RESET, 1);
    ioctl(info->si_fd, PERF_EVENT_IOC_REFRESH, 1);
    total+=counter;
}

void sample1(char** argv, int period)
{
    total= 0;
    pid_t pid= create_wrokload(argv);
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_sigaction = perf_event_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGIO, &sa, NULL) < 0)
        cerr << "Error setting up signal handler\n";

    perf_event_attr pe;
    memset(&pe, 0, sizeof(perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.sample_type = PERF_SAMPLE_IP;
    pe.sample_period = period;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    int fd = perf_event_open(&pe, pid, -1, -1, 0);
    if (fd == -1)
        cerr << "Error opening leader " << pe.config << endl;

    fcntl(fd, F_SETFL, O_NONBLOCK|O_ASYNC);
    fcntl(fd, F_SETSIG, SIGIO);
    fcntl(fd, F_SETOWN, getpid());

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);

// Start monitoring
    int status;
    waitpid(pid, &status, 0);
    ptrace(PTRACE_CONT, pid, 0, 0);
    while(1)
    {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
            break;
    }
// End monitoring
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    int64_t counter;
    read(fd, &counter, sizeof(int64_t));
    total+=counter;
    cout << total << endl;

    close(fd);
}

void sample2(char** argv, int period, int wk_events)
{
    total= 0;
    int64_t counter;
    pid_t pid= create_wrokload(argv);

    perf_event_attr pe;
    memset(&pe, 0, sizeof(perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.sample_type = PERF_SAMPLE_IP;
    pe.sample_period = period;
    pe.wakeup_events = wk_events;
    pe.mmap = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    int fd = perf_event_open(&pe, pid, -1, -1, 0);
    if (fd == -1)
        cerr << "Error opening leader " << pe.config << endl;

    perf_event_mmap_page* data;
    const int PAGE_SIZE= sysconf(_SC_PAGESIZE);
    const int DATA_SIZE= PAGE_SIZE;
    const int MMAP_SIZE= PAGE_SIZE+DATA_SIZE;
    data= (perf_event_mmap_page*)mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(data == MAP_FAILED)
        cerr << "Error open memory map" << endl;

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 1);

// Start monitoring
    int status;
    pollfd aux[1];
    aux[0].events= POLLIN;
    aux[0].fd= fd;
    waitpid(pid, &status, 0);
    ptrace(PTRACE_CONT, pid, 0, 0);
    while(1)
    {
        waitpid(pid, &status, WNOHANG);
        if (WIFEXITED(status))
            break;
        poll(aux, 1, 500);
        if(aux[0].revents & POLLIN)
        {
            read(fd, &counter, sizeof(int64_t));
            ioctl(fd, PERF_EVENT_IOC_RESET, 1);
            total+=counter;
        }
        if(aux[0].revents & POLLHUP)
        {

        }
    }
// End monitoring
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    read(fd, &counter, sizeof(int64_t));
    total+=counter;
    cout << total << endl;

    close(fd);
}

void sample3(char** argv, int period)
{
    total= 0;
    pid_t pid= create_wrokload(argv);

    perf_event_attr pe;
    memset(&pe, 0, sizeof(perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    int fd = perf_event_open(&pe, pid, -1, -1, 0);
    if (fd == -1)
        cerr << "Error opening leader " << pe.config << endl;

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 1);

// Start monitoring
    int status;
    waitpid(pid, &status, 0);
    ptrace(PTRACE_CONT, pid, 0, 0);
    while(1)
    {
        waitpid(pid, &status, WNOHANG);
        if (WIFEXITED(status))
            break;
        usleep(1.0/period*1e6);
        int64_t counter;
        read(fd, &counter, sizeof(int64_t));
        ioctl(fd, PERF_EVENT_IOC_RESET, 1);
        total+=counter;
    }
// End monitoring
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    int64_t counter;
    read(fd, &counter, sizeof(int64_t));
    total+=counter;
    cout << total << endl;

    close(fd);
}

int main(int argc, char** argv)
{
    sample1(argv, 10000);
    sample2(argv, 1000, 1000);
    sample3(argv, 100);
}