#ifndef Z3_MOCK_SIFRPC_H
#define Z3_MOCK_SIFRPC_H
#define SIF_RPC_M_NOWAIT 1
typedef struct { void *server; } SifRpcClientData_t;
int sceSifBindRpc(SifRpcClientData_t *, int, int);
int sceSifCheckStatRpc(SifRpcClientData_t *);
int sceSifCallRpc(SifRpcClientData_t *, int, int, void *, int, void *, int, void *, void *);
#endif
