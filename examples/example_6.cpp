#include "libcurlwrappersmtp.hpp"
#include "login_data.hpp"

#include <iostream>

const char* smtp_server = SERVER1;
const char* username = USERNAME1;
const char* password = PASSWORD1;
const char* from_name = FROMNAME1;
const char* from_email = FROMEMAIL1;

std::vector<std::tuple<std::string, std::string, std::string>> letters = {
    {"Subject 1", "Text 1",
     "<html><head><title>Title</title></head><body><i>Html "
     "1</i></body></html>"},
    {"Subject 2", "Text 2",
     "<html><head><title>Title</title></head><body><i>Html "
     "2</i></body></html>"},
    {"Subject 3", "Text 3",
     "<html><head><title>Title</title></head><body><i>Html "
     "3</i></body></html>"}};

std::vector<std::pair<std::string, std::string>> destinations = {
    {DESTINATIONNAME1, DESTINATIONEMAIL1}};

std::atomic<int> counter{0};

using namespace libcurlwrappersmtp;
LibCurlWrapperEmail EMAILER{};

void serve() {
  auto cb = [](Request& req) {
    if (!req.error.empty())
      std::cout << "Error: " << req.error << std::endl;
    else
      std::cout << "Done!" << std::endl;
    counter++;
  };
  for (const auto& data : letters) {
    EMAILER << server(smtp_server);
    EMAILER << user(username, password);
    EMAILER << from(from_name, from_email);
    for (const auto& dst : destinations)
      EMAILER << to(dst.first.c_str(), dst.second.c_str());
    EMAILER << subject(std::get<0>(data).c_str());
    EMAILER << cb;
    EMAILER << mimetext(std::get<1>(data).c_str());
    EMAILER << mimehtml(std::get<2>(data).c_str());
    EMAILER << directive::verbose;
    EMAILER << directive::asyncperform;
  }
}

int main() {
  std::array<std::thread, 10> threads{};
  for (auto& t : threads) t = std::thread(serve);
  for (auto& t : threads) t.join();
  while (counter < 30)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  return (EXIT_SUCCESS);
}
