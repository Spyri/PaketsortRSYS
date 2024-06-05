alias trun="make && sudo insmod test.ko && tail -f /var/log/syslog"
alias tstop="sudo rmmod test"
alias trestart="trun && tstop"
alias trunall="make && sudo insmod test.ko && sudo ./scanner"

