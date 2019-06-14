#pragma once
#include <fstream>
namespace GDBaseTests
{
	class Vec2
	{
	public:
		int x, y;

		Vec2() : x(0), y(0) {}
		Vec2(int xVal, int yVal) : x(xVal), y(yVal) {}
		Vec2(Vec2& v) : x(v.x), y(v.y) {}

		bool operator==(const Vec2& other)
		{
			return x == other.x && y == other.y;
		}
	};

	class Logger
	{
	public:
		Logger()
		{
			out_.open("log.txt");
		}
		
		explicit Logger(const char* out)
		{
			out_.open(out);
		}

		void log(const size_t txt)
		{
			out_ << txt;
			out_.flush();
		}
		
		void log(const int txt)
		{
			out_ << txt;
			out_.flush();
		}
		
		void log(const char* txt)
		{
			out_ << txt;
			out_.flush();
		}

		void log(std::string txt)
		{
			out_ << txt;
			out_.flush();
		}

		void logln(int txt)
		{
			out_ << txt << std::endl;
			out_.flush();
		}

		void logln(size_t txt)
		{
			out_ << txt << std::endl;
			out_.flush();
		}

		void logln(unsigned int txt)
		{
			out_ << txt << std::endl;
			out_.flush();
		}

		void logln(const char* txt)
		{
			out_ << txt << std::endl;
			out_.flush();
		}

		void logln(std::string txt)
		{
			out_ << txt << std::endl;
			out_.flush();
		}

		void nl()
		{
			out_ << std::endl;
			out_.flush();
		}

	private:
		std::ofstream out_;
	};
}