#pragma once

#include "types.hpp"

class GenericEnums {
	public:
		constexpr static uint32_t GETPORT_REQUEST_SIZE = 1024;
		constexpr static uint32_t GETPORT_RESPONSE_SIZE = 1024;
		constexpr static uint32_t CRED_REQUEST_SIZE = 1024;
		constexpr static uint32_t MOUNT_REQUEST_SIZE = 1024;

		DESC_CLASS_ENUM(AUTH_TYPE, uint32_t,
			None = -1,
			AUTH_INVALID = -1,
			AUTH_NONE = 0,
			AUTH_SYS = 1,
			AUTH_SHORT = 2, //Not implemented
			AUTH_DH = 3, //Not implemented
			RPCSEC_GSS = 6, //Not implemented
			UNKNOWN
		);

		DESC_CLASS_ENUM(RPCTYPE, uint32_t,
			None = -1,
			CALL = 0,
			REPLY = 1,
			UNKNOWN
		);

		DESC_CLASS_ENUM(RPC_VERSION, uint32_t,
			None = -1,
			RPC_VERSION1 = 1,
			RPC_VERSION2 = 2,
			UNKNOWN = 255
		);

		DESC_CLASS_ENUM(PROGRAM_VERSION, uint32_t,
			None = -1,
			PROGRAM_VERSION2 = 2,
			PROGRAM_VERSION3 = 3
		);

		DESC_CLASS_ENUM(RPC_PROGRAM, uint32_t,
			PORTMAP = 100000,
			NFS = 100003,
			MOUNT = 100005,
			NLM = 100021,
		);

		DESC_CLASS_ENUM(PROTOCOL_TYPE, uint32_t,
			IPPROTO_TCP = 6,
			IPPROTO_UDP = 17
		);

		DESC_CLASS_ENUM(MOUNT_VER, uint32_t,
			None = -1,
			VERSION3 = 3
		);

		DESC_CLASS_ENUM(MOUNTPROG, uint32_t,
			MOUNTPROC3_NULL = 0,
			MOUNTPROC3_MNT = 1,
			MOUNTPROC3_DUMP = 2,
			MOUNTPROC3_UMNT = 3,
			MOUNTPROC3_UMNTALL = 4,
			MOUNTPROC3_EXPORT = 5
		);

		DESC_CLASS_ENUM(MOUNTREPLY, uint32_t,
			MNT3_OK = 0,                    /* no error */
			MNT3ERR_PERM = 1,               /* Not owner */
			MNT3ERR_NOENT = 2,              /* No such file or directory */
			MNT3ERR_IO = 5,                 /* I/O error */
			MNT3ERR_ACCES = 13,             /* Permission denied */
			MNT3ERR_NOTDIR = 20,            /* Not a directory */
			MNT3ERR_INVAL = 22,             /* Invalid argument */
			MNT3ERR_NAMETOOLONG = 63,       /* Filename too long */
			MNT3ERR_NOTSUPP = 10004,        /* Operation not supported */
			MNT3ERR_SERVERFAULT = 10006    /* A failure on the server */
		);
};



