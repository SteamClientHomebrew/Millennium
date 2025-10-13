#pragma once
#include <sstream>

namespace millennium
{
class Cout
{
  public:
    template <typename T> Cout& operator<<(const T& value)
    {
        buffer_ << value;
        return *this;
    }

    Cout& operator<<(std::ostream& (*manip)(std::ostream&))
    {
        manip(buffer_);
        return *this;
    }

    std::string str() const
    {
        return buffer_.str();
    }

    void clear()
    {
        buffer_.str("");
        buffer_.clear();
    }

  private:
    std::ostringstream buffer_;
};

inline Cout cout;
} // namespace millennium