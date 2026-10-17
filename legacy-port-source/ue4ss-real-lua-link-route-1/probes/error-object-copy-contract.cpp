#include <cassert>
#include <cstring>
#include <string>
#include <ErrorObject.hpp>

int main() {
    RC::ErrorObject short_message("short");
    assert(std::strcmp(short_message.get_message(), "short") == 0);

    const std::string fits(1999, 'x');
    RC::ErrorObject fit_message(fits.c_str());
    assert(std::strlen(fit_message.get_message()) == 1999);
    assert(std::memcmp(fit_message.get_message(), fits.data(), fits.size()) == 0);

    const std::string equals_capacity(2000, 'y');
    RC::ErrorObject exact_capacity(equals_capacity.c_str());
    assert(std::strcmp(exact_capacity.get_message(), "An error occurred but no error message was supplied.") == 0);

    const std::string too_long(2001, 'z');
    RC::ErrorObject over_capacity(too_long.c_str());
    assert(std::strcmp(over_capacity.get_message(), "An error occurred but the message was too long for the buffer.") == 0);
}
