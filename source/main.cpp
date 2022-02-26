#include <fstream>
#include <iostream>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termio.h>
#include <thread>
#include <unistd.h>

#include <boost/lexical_cast.hpp>

static termios g_termConf;

void disableEcho()
{
    tcgetattr(STDIN_FILENO, &g_termConf);
    g_termConf.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termConf);
}

void enableEcho()
{
    tcgetattr(STDIN_FILENO, &g_termConf);
    g_termConf.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &g_termConf);
}

int kbhit()
{
    static constexpr int STDIN = 0;
    static bool init = false;
    if (!init) {
        tcgetattr(STDIN, &g_termConf);
        g_termConf.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &g_termConf);
        setbuf(stdin, NULL);
        init = true;
    }

    int bytes;
    ioctl(STDIN, FIONREAD, &bytes);
    return bytes;
}

int waitKey(int delay)
{
    delay /= 100;
    do {
        char key = 0;
        std::thread WaitKey([&]() {
            while (kbhit() != 0) {
                key = getchar();
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        WaitKey.detach();

        if (key != 0) {
            return key;
        }
    } while (--delay > 0);

    return -1;
}

int main()
{
    std::ifstream powerFile;
    unsigned totalCount = 0;
    unsigned sumofPower = 0;
    unsigned value = 0;

    disableEcho();
    while (true) {
        static constexpr int DELAY_MS = 1'000;
        const auto key = waitKey(DELAY_MS);
        static constexpr char ESCAPE_KEY = 27;
        if (key == ESCAPE_KEY || key == 'q' || key == 'Q') {
            break;
        } else if (key > 0) {
            sleep(1);
        }

        unsigned power = 0;
        std::string str;

        powerFile.open("/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power1_input");
        powerFile >> str;
        powerFile.close();
        value = str.empty() ? 0 : boost::lexical_cast<unsigned>(str);
        std::cout << "GPU Power:       " << value << " mW" << std::endl;
        power += value;
        str.clear();

        powerFile.open("/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power2_input");
        powerFile >> str;
        powerFile.close();
        value = str.empty() ? 0 : boost::lexical_cast<unsigned>(str);
        std::cout << "CPU Power:       " << value << " mW" << std::endl;
        power += value;
        str.clear();

        powerFile.open("/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power0_input");
        powerFile >> str;
        powerFile.close();
        value = str.empty() ? 0 : boost::lexical_cast<unsigned>(str);
        std::cout << "IN Power:        " << value << " mW" << std::endl;
        power += value;
        str.clear();

        std::cout << "Total Power:     " << power << " mW" << std::endl;
        sumofPower += power;
        totalCount++;
        std::cout << "Average Power:   " << sumofPower / totalCount << " mW" << std::endl;
        std::cout << std::endl;
    }
    enableEcho();

    return EXIT_SUCCESS;
}
