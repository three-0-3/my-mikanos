#pragma once

class Error {
	public:
		enum Code {
			kSuccess,
			kFull,
		};

		// Costructor
		Error(Code code) : code_{code} {}

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
};