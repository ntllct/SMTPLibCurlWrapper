#include "libcurlwrappersmtp.hpp"
#include "login_data.hpp"

#include <iostream>

std::vector<
    std::tuple<std::string, std::string, std::string, std::string, std::string>>
    accounts = {{SERVER1, USERNAME1, PASSWORD1, FROMNAME1, FROMEMAIL1},
                {SERVER2, USERNAME2, PASSWORD2, FROMNAME2, FROMEMAIL2},
                {SERVER3, USERNAME3, PASSWORD3, FROMNAME3, FROMEMAIL3},
                {SERVER4, USERNAME4, PASSWORD4, FROMNAME4, FROMEMAIL4}};
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
    {DESTINATIONNAME1, DESTINATIONEMAIL1},
    {DESTINATIONNAME2, DESTINATIONEMAIL2}};

std::atomic<int> counter{0};
std::atomic<int> thread_id{0};

using namespace libcurlwrappersmtp;
LibCurlWrapperEmail EMAILER{};

void serve() {
  int tid = thread_id++;
  size_t id = tid % accounts.size();

  auto cb = [](Request& req) {
    if (!req.error.empty())
      std::cout << "Error: " << req.error << std::endl;
    else
      std::cout << "Done!" << std::endl;
    counter++;
  };
  for (const auto& data : letters) {
    EMAILER << server(std::get<0>(accounts[id]).c_str());
    EMAILER << user(std::get<1>(accounts[id]).c_str(),
                    std::get<2>(accounts[id]).c_str());
    EMAILER << from(std::get<3>(accounts[id]).c_str(),
                    std::get<4>(accounts[id]).c_str());
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
