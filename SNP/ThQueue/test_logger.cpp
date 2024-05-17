#include "Logger.h"

int main(int argc, char** argv) {
    LogFile log_file{"log"};
    LogFile temp_file{"temp"};

    Logger logger{&log_file, "My cool", "Logger", std::format("{:x}", 0xdeadbeef)};
    Logger other_logger{&log_file, "Other", "Logger"};
    Logger temp_logger{&temp_file, "U!"};
    temp_logger.set_cout_enabled(false);
    Logger runtime_logger{"RuntimeOnly"};
    Logger other_logger_something = other_logger["Something"]["Else"][":D"];

    logger.info("Hello {}", 3);
    logger.warn("Hello {}", 3);
    logger.debug("Hello {}", 3);
    other_logger.info("Heil {}", "Ackmed");
    other_logger_something.info("Hi");
    logger.error("Hello {}", 3);
    temp_logger.info("Temp");
    runtime_logger.info("U");
    logger.fatal("Hello {}", 3);

    return 0;
}