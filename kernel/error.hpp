#pragma once

class Error {
	public:
		enum Code {
			kSuccess,
			kFull,
		};

		// Costructor
		Error(Code code, const char* file, int line) : code_{code}, line_{line}, file_{file} {}

		// To enable the expression such as
		// "auto err = AddDevice(bus, device, function, header_type)"
		operator bool() const {
			return this->code_ != kSuccess;
		}

		// To show the name in the error message
		const char* Name() const {
			return code_names_[static_cast<int>(this->code_)];
		}

	private:
		static constexpr std::array<const char*, 3> code_names_ = {
			"kSuccess",
			"kFull",
		};
		
		Code code_;
		int line_;
		const char* file_;
};

#define MAKE_ERROR(code) Error(code, __FILE__, __LINE__)