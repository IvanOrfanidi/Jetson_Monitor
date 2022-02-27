#include <fstream>
#include <iostream>
#include <map>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termio.h>
#include <thread>
#include <unistd.h>

#include <boost/lexical_cast.hpp>

enum class JetsonType {
    TX2,
    NANO,
    NANO_DEVELOPER_KIT,
    XAVIER_NX,
    XAVIER_NX_DEVELOPER_KIT,

    UNKNOWN
};

const std::map<std::string, JetsonType> JETSON_BOARD_ID_LIST = {
    { "p3310-1000", JetsonType::TX2 },
    { "p3448-0002", JetsonType::NANO },
    { "p3448-0000", JetsonType::NANO_DEVELOPER_KIT },
    { "p3509-0001", JetsonType::XAVIER_NX },
    { "p3509-0000", JetsonType::XAVIER_NX_DEVELOPER_KIT },
};

const std::map<JetsonType, std::string> JETSON_BOARD_NAME_LIST = {
    { JetsonType::TX2, "TX2" },
    { JetsonType::NANO, "Nano" },
    { JetsonType::NANO_DEVELOPER_KIT, "Nano (Developer Kit Version)" },
    { JetsonType::XAVIER_NX, "Xavier NX" },
    { JetsonType::XAVIER_NX_DEVELOPER_KIT, "Xavier NX (Developer Kit Version)" },
};

struct PowerFileName {
    const std::string GPU;
    const std::string CPU;
    const std::string IN;
};

const std::map<JetsonType, PowerFileName> JETSON_BOARD_POWER_FILE_LIST = {
    { JetsonType::TX2, {
                           "/sys/bus/i2c/drivers/ina3221x/0-0041/iio:device0/in_power1_input",
                           "/sys/bus/i2c/drivers/ina3221x/0-0041/iio:device0/in_power2_input",
                           "/sys/bus/i2c/drivers/ina3221x/0-0041/iio:device0/in_power0_input",
                       } },

    { JetsonType::NANO, {
                            "/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power1_input",
                            "/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power2_input",
                            "/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power0_input",
                        } },

    { JetsonType::NANO_DEVELOPER_KIT, {
                                          "/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power1_input",
                                          "/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power2_input",
                                          "/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power0_input",
                                      } },

    { JetsonType::XAVIER_NX, {
                                 "/sys/bus/i2c/drivers/ina3221x/7-0040/iio:device0/in_power1_input",
                                 "/sys/bus/i2c/drivers/ina3221x/7-0040/iio:device0/in_power2_input",
                                 "/sys/bus/i2c/drivers/ina3221x/7-0040/iio:device0/in_power0_input",
                             } },

    { JetsonType::XAVIER_NX_DEVELOPER_KIT, {
                                               "/sys/bus/i2c/drivers/ina3221x/7-0040/iio:device0/in_power1_input",
                                               "/sys/bus/i2c/drivers/ina3221x/7-0040/iio:device0/in_power2_input",
                                               "/sys/bus/i2c/drivers/ina3221x/7-0040/iio:device0/in_power0_input",
                                           } },
};

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

/**
 * It returns the code of the pressed key or -1 if no key was pressed before the specified time had elapsed.
 */
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
    std::string boardData;
    std::ifstream deviceFile;
    deviceFile.open("/proc/device-tree/nvidia,dtsfilename");
    deviceFile >> boardData;
    deviceFile.clear();

    JetsonType jetsonType = JetsonType::UNKNOWN;
    for (const auto& board : JETSON_BOARD_ID_LIST) {
        if (boardData.find(board.first) != std::string::npos) {
            jetsonType = board.second;
            break;
        }
    }
    if (jetsonType == JetsonType::UNKNOWN) {
        std::cerr << "error: unknown jetson board" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream powerFile;
    unsigned totalCount = 0;
    unsigned sumofPower = 0;
    unsigned value = 0;
    const PowerFileName powerFileName = std::move(JETSON_BOARD_POWER_FILE_LIST.find(jetsonType)->second);
    const std::string boardName = std::move(JETSON_BOARD_NAME_LIST.find(jetsonType)->second);

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

        std::cout << "NVIDIA Jetson Board: " << boardName << std::endl
                  << std::endl;

        unsigned power = 0;
        std::string str;

        powerFile.open(powerFileName.GPU);
        powerFile >> str;
        powerFile.close();
        value = str.empty() ? 0 : boost::lexical_cast<unsigned>(str);
        std::cout << "GPU Power:           " << value << " mW" << std::endl;
        power += value;
        str.clear();

        powerFile.open(powerFileName.CPU);
        powerFile >> str;
        powerFile.close();
        value = str.empty() ? 0 : boost::lexical_cast<unsigned>(str);
        std::cout << "CPU Power:           " << value << " mW" << std::endl;
        power += value;
        str.clear();

        powerFile.open(powerFileName.IN);
        powerFile >> str;
        powerFile.close();
        value = str.empty() ? 0 : boost::lexical_cast<unsigned>(str);
        std::cout << "IN Power:            " << value << " mW" << std::endl;
        power += value;
        str.clear();

        std::cout << "Total Power:         " << power << " mW" << std::endl;
        sumofPower += power;
        totalCount++;
        std::cout << "Average Power:       " << sumofPower / totalCount << " mW" << std::endl;
        std::cout << std::endl;
    }
    enableEcho();

    return EXIT_SUCCESS;
}
