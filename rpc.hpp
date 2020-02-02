#include "Context.hpp"

#pragma once

class RPC {
	public:
		DESC_CLASS_ENUM(RPC_PROGRAM_TYPE, int,
			PORTMAP,
			NFS,
			MOUNT,
			NLM,
			UNKNOWN
		);
		RPC() = default;

		int makeRPC();

		int runRPC();

	private:
		Context_p context;
};
