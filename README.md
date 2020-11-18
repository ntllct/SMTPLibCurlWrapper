# SMTPLibCurlWrapper

Easy to use wrapper around libcurl to send emails.

This library allows you:
* You can reuse connections to send many emails without reconnection.
* You can send emails from different threads.
* You can send text and HTML version in a single letter.
* You can send files.

Current version uses a single thread to send emails. This means if you have a connection delay, all other emails, that you want to send through different servers, will be delayed too.

It is easy to send email:
```
#include "libcurlwrappersmtp.hpp"
#include "login_data.hpp"

#include <iostream>

int main() {
  using namespace libcurlwrappersmtp;
  LibCurlWrapperEmail EMAILER{};

  void (*cb)(Request&) = [](Request& req) {
    if (!req.error.empty())
      std::cout << "Error: " << req.error << std::endl;
    else
      std::cout << "Done!" << std::endl;
  };
  EMAILER << server("smtp.gmail.com:587");
  EMAILER << user("example@gmail.com", "password");
  EMAILER << from("My Name", "<example@gmail.com>");
  EMAILER << to("Name", "<example@gmail.com>");
  EMAILER << subject("Subject");
  EMAILER << cb;
  EMAILER << mimetext("message");
  EMAILER << directive::syncperform;
  return (EXIT_SUCCESS);
}
```

## Build

Before building examples you have to edit [examples/login_data.hpp] and type relevant informations for servers.
After this from project folder type:
```
mkdir build
cd build
cmake ..
make
```

Don't forget to add libcurl and pthread to your compiler.
```
g++ -std=c++17 -m64 -O3 -mavx -Wall -pedantic-errors -Wold-style-cast -Weffc++ main.cpp -o main -lpthread -lcurl
```

Run examples by:
```
./example_1
./example_2
./example_3
./example_4
./example_5
./example_6
./example_7
```

Please, read rules for SMTP server, that you want to use. It may reject your connection if you didn't allow this on a settings page.\
For gmail you have to enable less secure apps on this page: https://myaccount.google.com/lesssecureapps\
For www.yahoo.com or www.yandex.com you have to add your application and generate password for it on a security page. A new password will be different from your account password.

# Examples:

[Send a text email](examples/example_1.cpp)\
[Send an HTML email](examples/example_2.cpp)\
[Send two version in a single email](examples/example_3.cpp)\
[Send a few letters through a single connection](examples/example_4.cpp)\
[Send two files](examples/example_5.cpp)\
[Send many emails from different threads](examples/example_6.cpp)\
[Send many emails from different servers and threads](examples/example_7.cpp)
