#ifndef APP_ERROR_HTTP_H
#define APP_ERROR_HTTP_H

#include "error.h"
#include <string>

class ErrorHttp : public Error {
public:
    explicit ErrorHttp(const std::string& mensaje, long codigo_http = 0)
        : Error(mensaje)
        , codigo_http_(codigo_http) {
    }

    [[nodiscard]] long codigo_http() const noexcept {
        return codigo_http_;
    }

private:
    long codigo_http_{ 0 };
};

#endif // APP_ERROR_HTTP_H

