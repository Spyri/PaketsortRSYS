#include <sys/resource.h>
#include <stdio.h>

int main() {
    struct rlimit rl;
    rl.rlim_cur = 100000; // Set CPU time limit in microseconds
    rl.rlim_max = 100000; // Set CPU time limit in microseconds

    if (setrlimit(RLIMIT_RTTIME, &rl) != 0) {
        perror("setrlimit");
        return 1;
    }

    // Your real-time code here

    return 0;
}
