#pragma once

#include <condition_variable>
#include <cstring>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include <curl/curl.h>

namespace libcurlwrappersmtp {

/* Servers:
 * smtp.gmail.com:587
 * smtps://smtp.yandex.com:465
 * smtps://smtp.mail.yahoo.com:465
 * */

// cURL API:
// https://curl.se/libcurl/c/smtp-mime.html

struct user {
  const char* name{nullptr};
  const char* pass{nullptr};
  user() = delete;
  user(const char* n, const char* p) : name(n), pass(p) {}
};
struct server {
  const char* name{nullptr};
  server() = delete;
  server(const char* u) : name(u) {}
};
struct userdata {
  void* ptr{nullptr};
  userdata() = delete;
  userdata(void* data) : ptr(data) {}
};
struct mimehtml {
  const char* data{nullptr};
  mimehtml() = delete;
  mimehtml(const char* d) : data(d) {}
};
struct mimetext {
  const char* data{nullptr};
  mimetext() = delete;
  mimetext(const char* d) : data(d) {}
};
struct mimefile {
  const char* filename{nullptr};
  mimefile() = delete;
  mimefile(const char* d) : filename(d) {}
};
struct from {
  const char* name{nullptr};
  const char* email{nullptr};
  from() = delete;
  from(const char* fn, const char* fa) : name(fn), email(fa) {}
};
struct to {
  const char* name{nullptr};
  const char* email{nullptr};
  to() = delete;
  to(const char* fn, const char* fa) : name(fn), email(fa) {}
};
struct subject {
  const char* subj{nullptr};
  subject() = delete;
  subject(const char* u) : subj(u) {}
};

enum class directive : unsigned char {
  syncperform,
  asyncperform,
  verbose,
};

class KeepAliveServers;
class LibCurlWrapperEmail;
struct Request {
  std::string error{};
  void* user_data{nullptr};

  Request() = default;
  Request& operator=(const Request&) = default;
  Request& operator=(Request&&) = default;
  Request(const Request&) = default;
  Request(Request&&) = default;
  ~Request() {
    if (recipients != nullptr) curl_slist_free_all(recipients);
    if (headers != nullptr) curl_slist_free_all(headers);
    if (mime != nullptr) curl_mime_free(mime);
  }

 private:
  friend KeepAliveServers;
  friend LibCurlWrapperEmail;
  CURL* curl{nullptr};
  std::packaged_task<void(Request&)> cb{[](Request& req) {}};

  long verbose{0};

  CURLcode result{};

  std::vector<std::pair<std::string, std::string>> to_addresses{};
  std::vector<std::string> filenames{};
  std::pair<std::string, std::string> from_address{};
  std::string smtp_server{};  // SMTP server
  std::string sendtext{};
  std::string sendhtml{};
  std::string username{};
  std::string password{};
  std::string email_subject{};

  struct curl_slist* recipients{nullptr};
  struct curl_slist* headers{nullptr};
  struct curl_slist* headers2{nullptr};

  curl_mime* alt{nullptr};
  curl_mime* mime{nullptr};  // HTML part
  curl_mimepart* mimepart{nullptr};

  bool is_data_valid() {
    if (sendtext.empty() && sendhtml.empty()) {
      error.assign("No message for body!");
      return (false);
    }
    return (true);
  }
  // Init curl and set some options for server
  void init() {
    // Cleanup will be made from KeepAliveServers
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, smtp_server.c_str());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    // curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    // prevent infinite reconnection loop
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
  }
  // Set some options for this email
  void set_options() {
    curl_easy_setopt(curl, CURLOPT_PRIVATE, user_data);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from_address.second.c_str());
    for (const auto& to_address : to_addresses)
      recipients = curl_slist_append(recipients, to_address.second.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, verbose);
  }
  void build_headers() {
    static std::mt19937_64 random64(time(nullptr));
    char buffer[1024];
    time_t tt_ = time(nullptr);
    struct tm* timeinfo_ = localtime(&tt_);
    memset(buffer, 0, 1024);
    strftime(buffer, 79, "Date: %a, %e %b %Y %T %z", timeinfo_);
    headers = curl_slist_append(headers, buffer);

    std::string header_;
    header_.reserve(256);

    header_ = "From: ";
    if (!from_address.first.empty()) {
      header_.append(from_address.first);
      header_.push_back(' ');
    }
    header_.append(from_address.second);
    headers = curl_slist_append(headers, header_.c_str());

    header_ = "To: ";
    bool need_sep_ = false;
    for (const auto& to_address : to_addresses) {
      if (need_sep_) header_.append(", ");
      if (!to_address.first.empty()) {
        header_.append(to_address.first);
        header_.push_back(' ');
      }
      header_.append(to_address.second);
      need_sep_ = true;
    }
    headers = curl_slist_append(headers, header_.c_str());

    header_ = "Message-ID: <";
    uint64_t random_message_id_[2] = {random64(), random64()};
    const char* in_ = reinterpret_cast<const char*>(random_message_id_);
    const char* hex_char = "0123456789abcdef";
    unsigned char buf;
    unsigned char mask = 0x0F;
    for (int i = 0; i < 16; i++) {
      if (i == 4) header_.push_back('-');
      if (i == 6) header_.push_back('-');
      if (i == 8) header_.push_back('-');
      if (i == 10) header_.push_back('-');
      buf = *in_;
      buf >>= 4;
      buf &= mask;
      header_.push_back(hex_char[buf]);
      buf = *in_++;
      buf &= mask;
      header_.push_back(hex_char[buf]);
    }
    std::string domain_("@example.org");
    std::string::size_type pos_ = from_address.second.find("@");
    if (pos_ != std::string::npos) {
      domain_.clear();
      for (auto it_ = from_address.second.cbegin() + pos_;
           it_ != from_address.second.cend(); ++it_) {
        if (*it_ == '>') break;
        if (isspace(*it_)) break;
        domain_.push_back(*it_);
      }
    }
    header_.append(domain_);
    header_.push_back('>');
    headers = curl_slist_append(headers, header_.c_str());

    header_ = "Subject: ";
    header_.append(email_subject);
    headers = curl_slist_append(headers, header_.c_str());

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  void build_body() {
    mime = curl_mime_init(curl);
    alt = curl_mime_init(curl);

    // Order "mimetype" is important.
    if (!sendtext.empty()) {
      mimepart = curl_mime_addpart(alt);
      curl_mime_data(mimepart, sendtext.c_str(), CURL_ZERO_TERMINATED);
      curl_mime_type(mimepart, "text/plain");
    }
    if (!sendhtml.empty()) {
      mimepart = curl_mime_addpart(alt);
      curl_mime_data(mimepart, sendhtml.c_str(), CURL_ZERO_TERMINATED);
      curl_mime_type(mimepart, "text/html");
    }
    mimepart = curl_mime_addpart(mime);
    curl_mime_subparts(mimepart, alt);
    curl_mime_type(mimepart, "multipart/alternative");

    // You do not need to call curl_slist_free_all for headers2
    headers2 = curl_slist_append(nullptr, "Content-Disposition: inline");
    curl_mime_headers(mimepart, headers2, 1);

    for (const auto& filename : filenames) {
      mimepart = curl_mime_addpart(mime);
      curl_mime_filedata(mimepart, filename.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  }
  // Send email
  void perform() {
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) error.assign(curl_easy_strerror(res));
  }
  void callback() { cb(*this); }
};
// For keep-alive connections
class KeepAliveServers {
 public:
  void init_and_lock(Request& req) {
    size_t uhash = std::hash<std::string>{}(req.username);
    size_t phash = std::hash<std::string>{}(req.password);
    std::unique_lock<std::mutex> lck_(_servers_mtx);
    auto it_ = _servers.find(req.smtp_server);
    if (it_ != _servers.end()) {
      auto itlow = _servers.lower_bound(req.smtp_server);
      auto itup = _servers.upper_bound(req.smtp_server);

      for (it_ = itlow; it_ != itup; ++it_) {
        if (it_->second->username_hash == uhash &&
            it_->second->password_hash == phash)
          break;
      }
      if (it_ == itup) it_ = _servers.end();
    }
    if (it_ == _servers.end()) {  // Init new
      req.init();
      auto serv = _servers.emplace(req.smtp_server,
                                   new ServerData(req.curl, uhash, phash));
      serv->second->mtx.lock();
    } else {  // Get existing
      req.curl = it_->second->curl;
      it_->second->last_connection = time(nullptr);
      it_->second->mtx.lock();
    }
  }
  void unlock(const Request& req) {
    std::unique_lock<std::mutex> lck_(_servers_mtx);
    auto it_ = _servers.find(req.smtp_server);
    if (it_ != _servers.end()) {
      size_t uhash = std::hash<std::string>{}(req.username);
      size_t phash = std::hash<std::string>{}(req.password);
      auto itlow = _servers.lower_bound(req.smtp_server);
      auto itup = _servers.upper_bound(req.smtp_server);

      for (it_ = itlow; it_ != itup; ++it_) {
        if (it_->second->username_hash == uhash &&
            it_->second->password_hash == phash) {
          it_->second->last_connection = time(nullptr);
          it_->second->mtx.unlock();
          break;
        }
      }
    }
  }
  void clear_old() {
    // Expiries keys will be erased
    time_t expiries_ = time(nullptr) - 15;
    std::unique_lock<std::mutex> lck_(_servers_mtx);
    for (auto it_ = _servers.begin(); it_ != _servers.end();) {
      if (it_->second->last_connection < expiries_ &&
          it_->second->mtx.try_lock()) {
        curl_easy_cleanup(it_->second->curl);
        it_->second->mtx.unlock();
        it_ = _servers.erase(it_);
      } else {
        it_++;
      }
    }
  }
  ~KeepAliveServers() {
    std::unique_lock<std::mutex> lck_(_servers_mtx);
    for (const auto& serv : _servers) curl_easy_cleanup(serv.second->curl);
  }

 private:
  struct ServerData {
    time_t last_connection{time(nullptr)};
    CURL* curl{nullptr};
    size_t username_hash;
    size_t password_hash;
    std::mutex mtx{};

    ServerData() = delete;
    ServerData& operator=(const ServerData&) = default;
    ServerData& operator=(ServerData&&) = default;
    ServerData(const ServerData&) = default;
    ServerData(ServerData&&) = default;
    ServerData(CURL* c, size_t uhash, size_t phash)
        : curl(c), username_hash(uhash), password_hash(phash) {}
  };
  std::mutex _servers_mtx{};
  std::multimap<std::string, std::unique_ptr<ServerData>> _servers{};
};

class LibCurlWrapperEmail {
 public:
  size_t globalSize() const noexcept { return (globalRequests.size()); }
  size_t localSize() const noexcept { return (localRequests.size()); }
  // class control
  LibCurlWrapperEmail& operator<<(directive d) {
    if (localRequests.empty()) return (*this);
    switch (d) {
      case directive::syncperform:
        _perform(localRequests);
        localRequests.clear();
        break;
      case directive::asyncperform:
        mtxRequests.lock();
        for (auto& r : localRequests) globalRequests.push_back(std::move(r));
        mtxRequests.unlock();
        cV.notify_one();
        localRequests.clear();
        break;
      case directive::verbose:
        localRequests.back()->verbose = 1;
        break;
      default:
        break;
    }
    return (*this);
  }
  // New address
  LibCurlWrapperEmail& operator<<(const server&& s) {
    localRequests.emplace_back(new Request);
    localRequests.back()->smtp_server.assign(s.name);
    localRequests.back()->email_subject.assign("No subject.");
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const from&& f) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->from_address.first.assign(f.name);
    localRequests.back()->from_address.second.assign(f.email);
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const to&& t) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->to_addresses.emplace_back(t.name, t.email);
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const subject&& s) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->email_subject.assign(s.subj);
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const user&& u) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->username.assign(u.name);
    localRequests.back()->password.assign(u.pass);
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(std::packaged_task<void(Request&)>&& cb) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->cb =
        std::forward<std::packaged_task<void(Request&)>>(cb);
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(void (*cb)(Request&)) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->cb = std::packaged_task<void(Request&)>(cb);
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const mimetext& data) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->sendtext = data.data;
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const mimehtml& data) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->sendhtml = data.data;
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const mimefile& data) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->filenames.emplace_back(data.filename);
    return (*this);
  }
  LibCurlWrapperEmail& operator<<(const userdata& uid) {
    if (localRequests.empty()) return (*this);
    localRequests.back()->user_data = uid.ptr;
    return (*this);
  }
  LibCurlWrapperEmail() {
    size_t old = nInstances++;
    if (old == 0) curl_global_init(CURL_GLOBAL_DEFAULT);
    run();
  }
  ~LibCurlWrapperEmail() {
    auto old = --nInstances;
    if (old == 0) curl_global_cleanup();
    stop();
  }

 private:
  void run() {
    bool needRun = !isRunning.exchange(true);
    if (!needRun) return;
    crawlerThread = std::thread(LibCurlWrapperEmail::_serve, this);
  }
  void stop() {
    bool needJoin = isRunning.exchange(false);
    cV.notify_all();
    if (needJoin) {
      if (crawlerThread.joinable()) crawlerThread.join();
    }
  }

  static void _serve(LibCurlWrapperEmail* this_) {
    time_t next_clear_old_ = time(nullptr) + 3;
    while (isRunning) {
      std::unique_lock<std::mutex> cv_lock_(cvMtx);
      cV.wait_for(cv_lock_, std::chrono::seconds(1),
                  [this_] { return (!globalRequests.empty() || !isRunning); });
      // Clear old
      if (next_clear_old_ <= time(nullptr)) {
        this_->_servers.clear_old();
        next_clear_old_ = time(nullptr) + 3;
      }

      if (!isRunning) break;

      if (globalRequests.empty()) continue;

      mtxRequests.lock();
      auto reqs = std::move(globalRequests);
      globalRequests.clear();
      mtxRequests.unlock();
      if (!reqs.empty()) this_->_perform(reqs);
    }
  }

  // Thread for async performs
  static inline std::thread crawlerThread{};
  // Local running flag
  static inline std::atomic<bool> isRunning{false};
  // How much instances of class created. For global init/cleanup.
  static inline std::atomic<size_t> nInstances{0};

  // Each thread has its own structure
  thread_local static inline std::vector<std::unique_ptr<Request>>
      localRequests{};
  // For async perform
  static inline std::vector<std::unique_ptr<Request>> globalRequests{};
  // Each thread has its own structure
  thread_local static inline std::unique_ptr<Request> _request{};
  // For working with queue
  static inline std::mutex mtxRequests{};
  // For async queries
  static inline std::mutex cvMtx{};
  static inline std::condition_variable cV{};

  static inline KeepAliveServers _servers{};

  //
  void _perform(std::vector<std::unique_ptr<Request>>& req) const noexcept {
    if (req.empty()) return;
    for (auto& r : req) _perform_once(*r);
  }
  //
  void _perform_once(Request& req) const noexcept {
    if (req.is_data_valid()) {
      _servers.init_and_lock(req);
      try {
        req.set_options();
        req.build_headers();
        req.build_body();
        req.perform();
      } catch ([[maybe_unused]] const std::exception& e) {
      }
      _servers.unlock(req);
    }
    try {
      req.callback();
    } catch ([[maybe_unused]] const std::exception& e) {
    }
  }
};

}  // namespace libcurlwrappersmtp
