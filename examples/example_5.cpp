#include "libcurlwrappersmtp.hpp"
#include "login_data.hpp"

#include <fstream>
#include <iostream>

const char* smtp_server = SERVER1;
const char* username = USERNAME1;
const char* password = PASSWORD1;
const char* from_name = FROMNAME1;
const char* from_email = FROMEMAIL1;

const char* smtp_subject = "Hi!";
const char* smtp_text = "text";
const char* smtp_html =
    "<html><head><title>Title</title></head><body><i>html</i></body></html>";

std::vector<std::pair<std::string, std::string>> destinations = {
    {DESTINATIONNAME1, DESTINATIONEMAIL1}};

int main() {
  using namespace libcurlwrappersmtp;
  LibCurlWrapperEmail EMAILER{};

  std::ofstream ofs1("file1.txt");
  if (!ofs1) {
    std::cout << "Cannot create file1.txt" << std::endl;
    return (EXIT_FAILURE);
  }
  ofs1 << "content 1" << std::endl;
  ofs1.close();

  std::ofstream ofs2("file2.txt");
  if (!ofs2) {
    std::cout << "Cannot create file2.txt" << std::endl;
    return (EXIT_FAILURE);
  }
  ofs2 << "content 2" << std::endl;
  ofs2.close();

  void (*cb)(Request&) = [](Request& req) {
    if (!req.error.empty())
      std::cout << "Error: " << req.error << std::endl;
    else
      std::cout << "Done!" << std::endl;
  };
  EMAILER << server(smtp_server);
  EMAILER << user(username, password);
  EMAILER << from(from_name, from_email);
  for (const auto& dst : destinations)
    EMAILER << to(dst.first.c_str(), dst.second.c_str());
  EMAILER << subject(smtp_subject);
  EMAILER << cb;
  EMAILER << mimetext(smtp_text);
  EMAILER << mimehtml(smtp_html);
  EMAILER << mimefile("file1.txt");
  EMAILER << mimefile("file2.txt");
  // Comment this line if you don't want debug information
  EMAILER << directive::verbose;
  EMAILER << directive::syncperform;
  return (EXIT_SUCCESS);
}
