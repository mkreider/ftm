/** @file etherbone.h
 *  @brief The public API of the Etherbone library.
 *
 *  Copyright (C) 2011-2012 GSI Helmholtz Centre for Heavy Ion Research GmbH 
 *
 *  All Etherbone object types are opaque in this interface.
 *  Only those methods listed in this header comprise the public interface.
 *
 *  @author Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 *  @bug None!
 *
 *******************************************************************************
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************
 */

#ifndef ETHERBONE_H
#define ETHERBONE_H

#define DEBUG_EB 		1

#define EB_PROTOCOL_VERSION	1
#define EB_ABI_VERSION		0x02	/* incremented on incompatible changes */

#include <stdint.h>   /* uint32_t ... */
#include <inttypes.h> /* EB_DATA_FMT ... */

/* Symbol visibility definitions */
#ifdef __WIN32
#ifdef ETHERBONE_IMPL
#define EB_PUBLIC __declspec(dllexport)
#define EB_PRIVATE
#else
#define EB_PUBLIC __declspec(dllimport)
#define EB_PRIVATE
#endif
#else
#define EB_PUBLIC
#define EB_PRIVATE __attribute__((visibility("hidden")))
#endif

/* Pointer type -- depends on memory implementation */
#ifdef EB_USE_MALLOC
#define EB_POINTER(typ) struct typ*
#define EB_NULL 0
#define EB_MEMORY_MODEL 0x0001U
#else
#define EB_POINTER(typ) uint16_t
#define EB_NULL ((uint16_t)-1)
#define EB_MEMORY_MODEL 0x0000U
#endif

/* Opaque structural types */
typedef EB_POINTER(eb_socket)    eb_socket_t;
typedef EB_POINTER(eb_device)    eb_device_t;
typedef EB_POINTER(eb_cycle)     eb_cycle_t;
typedef EB_POINTER(eb_operation) eb_operation_t;

/* Configurable maximum bus width supported */
#if defined(EB_FORCE_64)
typedef uint64_t eb_address_t;
typedef uint64_t eb_data_t;
#define EB_ADDR_FMT PRIx64
#define EB_DATA_FMT PRIx64
#define EB_DATA_C UINT64_C
#define EB_ADDR_C UINT64_C
#elif defined(EB_FORCE_32)
typedef uint32_t eb_address_t;
typedef uint32_t eb_data_t;
#define EB_ADDR_FMT PRIx32
#define EB_DATA_FMT PRIx32
#define EB_DATA_C UINT32_C
#define EB_ADDR_C UINT32_C
#elif defined(EB_FORCE_16)
typedef uint16_t eb_address_t;
typedef uint16_t eb_data_t;
#define EB_ADDR_FMT PRIx16
#define EB_DATA_FMT PRIx16
#define EB_DATA_C UINT16_C
#define EB_ADDR_C UINT16_C
#else
/* The default maximum width is the machine word-size */
typedef uintptr_t eb_address_t;
typedef uintptr_t eb_data_t;
#define EB_ADDR_FMT PRIxPTR
#define EB_DATA_FMT PRIxPTR
#define EB_DATA_C UINT64_C
#define EB_ADDR_C UINT64_C
#endif

/* Identify the library ABI this header must match */
#define EB_BUS_MODEL	(0x0010U * sizeof(eb_address_t)) + (0x0001U * sizeof(eb_data_t))
#define EB_ABI_CODE	((EB_ABI_VERSION << 8) + EB_BUS_MODEL + EB_MEMORY_MODEL)

/* Status codes */
typedef int eb_status_t;
#define EB_OK		0
#define EB_FAIL		-1
#define EB_ADDRESS	-2
#define EB_WIDTH	-3
#define EB_OVERFLOW	-4
#define EB_ENDIAN	-5
#define EB_BUSY		-6
#define EB_TIMEOUT	-7
#define EB_OOM          -8
#define EB_ABI		-9

/* A bitmask containing values from EB_DATAX | EB_ADDRX */
typedef uint8_t eb_width_t;
/* A bitmask containing values from EB_DATAX | EB_ENDIAN_MASK */
typedef uint8_t eb_format_t;

#define EB_DATA8	0x01
#define EB_DATA16	0x02
#define EB_DATA32	0x04
#define EB_DATA64	0x08
#define EB_DATAX	0x0f

#define EB_ADDR8	0x10
#define EB_ADDR16	0x20
#define EB_ADDR32	0x40
#define EB_ADDR64	0x80
#define EB_ADDRX	0xf0

#define EB_ENDIAN_MASK	0x30
#define	EB_BIG_ENDIAN	0x10
#define EB_LITTLE_ENDIAN 0x20

#define EB_DESCRIPTOR_IN  0x01
#define EB_DESCRIPTOR_OUT 0x02

/* Callback types */
typedef void *eb_user_data_t;
typedef void (*eb_callback_t )(eb_user_data_t, eb_device_t, eb_operation_t, eb_status_t);

typedef int eb_descriptor_t;
typedef int (*eb_descriptor_callback_t)(eb_user_data_t, eb_descriptor_t, uint8_t mode); /* mode = EB_DESCRIPTOR_IN | EB_DESCRIPTOR_OUT */

/* Type of the SDB record */
enum sdb_record_type {
  sdb_interconnect = 0x00,
  sdb_device       = 0x01,
  sdb_bridge       = 0x02,
  sdb_integration  = 0x80,
  sdb_empty        = 0xFF,
};

/* The type of bus (specifies bus-specific fields) */
enum sdb_bus_type {
  sdb_wishbone = 0x00
};

/* 40 bytes, 8-byte alignment */
struct sdb_product {
  uint64_t vendor_id;   /* Vendor identifier */
  uint32_t device_id;   /* Vendor assigned device identifier */
  uint32_t version;     /* Device-specific version number */
  uint32_t date;        /* Hex formatted release date (eg: 0x20120501) */
  int8_t   name[19];    /* ASCII. no null termination. */
  uint8_t  record_type; /* sdb_record_type */
};

/* 56 bytes, 8-byte alignment */
struct sdb_component {
  uint64_t           addr_first; /* Address range: [addr_first, addr_last] */
  uint64_t           addr_last;
  struct sdb_product product;
};

/* Record type: sdb_empty */
typedef struct sdb_empty {
  int8_t  reserved[63];
  uint8_t record_type;
} *sdb_empty_t;

/* Record type: sdb_interconnect
 * This header prefixes every SDB table.
 * It's component describes the interconnect root complex/bus/crossbar.
 */
typedef struct sdb_interconnect {
  uint32_t             sdb_magic;    /* 0x5344422D */
  uint16_t             sdb_records;  /* Length of the SDB table (including header) */
  uint8_t              sdb_version;  /* 1 */
  uint8_t              sdb_bus_type; /* sdb_bus_type */
  struct sdb_component sdb_component;
} *sdb_interconnect_t;

/* Record type: sdb_integration
 * This meta-data record describes the aggregate product of the bus.
 * For example, consider a manufacturer which takes components from 
 * various vendors and combines them with a standard bus interconnect.
 * The integration component describes aggregate product.
 */
typedef struct sdb_integration {
  int8_t             reserved[24];
  struct sdb_product product;
} *sdb_integration_t;

/* Flags used for wishbone bus' in the bus_specific field */
#define SDB_WISHBONE_WIDTH          0xf
#define SDB_WISHBONE_LITTLE_ENDIAN  0x80

/* Record type: sdb_device
 * This component record describes a device on the bus.
 * abi_class describes the published standard register interface, if any.
 */
typedef struct sdb_device {
  uint16_t             abi_class; /* 0 = custom device */
  uint8_t              abi_ver_major;
  uint8_t              abi_ver_minor;
  uint32_t             bus_specific;
  struct sdb_component sdb_component;
} *sdb_device_t;

/* Record type: sdb_bridge
 * This component describes a bridge which embeds a nested bus.
 * This does NOT include bus controllers, which are *devices* that
 * indirectly control a nested bus.
 */
typedef struct sdb_bridge {
  uint64_t             sdb_child; /* Nested SDB table */
  struct sdb_component sdb_component;
} *sdb_bridge_t;

/* All possible SDB record structure */
typedef union sdb_record {
  struct sdb_empty        empty;
  struct sdb_device       device;
  struct sdb_bridge       bridge;
  struct sdb_integration  integration;
  struct sdb_interconnect interconnect;
} *sdb_record_t;

/* Complete bus description */
typedef struct sdb {
  struct sdb_interconnect interconnect;
  union  sdb_record       record[1]; /* bus.sdb_records-1 elements (not 1) */
} *sdb_t;

/* Handler descriptor */
typedef struct eb_handler {
  /* This pointer must remain valid until after you detach the device */
  sdb_device_t device;
  
  eb_user_data_t data;
  eb_status_t (*read) (eb_user_data_t, eb_address_t, eb_width_t, eb_data_t*);
  eb_status_t (*write)(eb_user_data_t, eb_address_t, eb_width_t, eb_data_t);
} *eb_handler_t;

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*                                 C99 API                                  */
/****************************************************************************/

extern int mprintf(char const *format, ...);

#define dbgprint(fmt, ...) \
            do { if (DEBUG_EB) mprintf(fmt, ##__VA_ARGS__); } while (0)

/* Convert status to a human-readable printable string */
EB_PUBLIC
const char* eb_status(eb_status_t code);

/* Open an Etherbone socket for communicating with remote devices.
 * Open sockets must be hooked into an event loop; see eb_socket_{run,check}.
 * 
 * The abi_code must be EB_ABI_CODE. This confirms library compatability.
 * The port parameter is optional; 0 lets the operating system choose.
 * Supported_widths list bus widths acceptable to the local Wishbone bus.
 *   EB_ADDR32|EB_ADDR8|EB_DATAX means 8/32-bit addrs and 8/16/32/64-bit data.
 *   Devices opened by this socket only negotiate a subset of these widths.
 *   Virtual slaves attached to the socket never see a width excluded here.
 *
 * Return codes:
 *   OK		- successfully open the socket port
 *   FAIL	- operating system forbids access
 *   BUSY	- specified port is in use (only possible if port != 0)
 *   WIDTH      - supported_widths were invalid
 *   OOM        - out of memory
 *   ABI        - library is not compatible with application
 */
EB_PUBLIC
eb_status_t eb_socket_open(uint16_t      abi_code,
                           const char*   port, 
                           eb_width_t    supported_widths,
                           eb_socket_t*  result);

/* Close the Etherbone socket.
 * Any use of the socket after successful close will probably segfault!
 *
 * Return codes:
 *   OK		- successfully closed the socket and freed memory
 *   BUSY	- there are open devices on this socket
 */
EB_PUBLIC
eb_status_t eb_socket_close(eb_socket_t socket);

/* Wait for an event on the socket and process it.
 * This function is useful if your program has no event loop of its own.
 * If timeout_us == 0, return immediately. If timeout_us == -1, wait forever.
 * It returns the time expended while waiting.
 */
EB_PUBLIC
int eb_socket_run(eb_socket_t socket, int timeout_us);

/* Integrate this Etherbone socket into your own event loop.
 *
 * You must call eb_socket_check whenever:
 *   1. An etherbone timeout expires (eb_socket_timeout tells you when this is)
 *   2. An etherbone socket is ready to read (eb_socket_descriptors lists them)
 * You must provide eb_socket_check with:
 *   1. The current time
 *   2. A function that returns '1' if a socket is ready to read
 *
 * YOU MAY NOT CLOSE OR MODIFY ETHERBONE SOCKET DESCRIPTORS IN ANY WAY.
 */
EB_PUBLIC
void eb_socket_check(eb_socket_t socket, uint32_t now, eb_user_data_t user, eb_descriptor_callback_t ready);

/* Calls (*list)(user, fd) for every descriptor the socket uses. */
EB_PUBLIC
void eb_socket_descriptors(eb_socket_t socket, eb_user_data_t user, eb_descriptor_callback_t list); 

/* Returns 0 if there are no timeouts pending, otherwise the time in UTC seconds. */
EB_PUBLIC
uint32_t eb_socket_timeout(eb_socket_t socket);

/* Add a device to the virtual bus.
 * This handler receives all reads and writes to the specified address.
 * The handler structure passed to eb_socket_attach need not be preserved.
 * The sdb_device MUST be preserved until the device is detached.
 * NOTE: the address range [0x0, 0x4000) is reserved for internal use.
 *
 * Return codes:
 *   OK         - the handler has been installed
 *   OOM        - out of memory
 *   ADDRESS    - the specified address range overlaps an existing device.
 */
EB_PUBLIC
eb_status_t eb_socket_attach(eb_socket_t socket, eb_handler_t handler);

/* Detach the device from the virtual bus.
 *
 * Return codes:
 *   OK         - the devices has be removed
 *   ADDRESS    - there is no device at the specified address.
 */
EB_PUBLIC
eb_status_t eb_socket_detach(eb_socket_t socket, sdb_device_t device);

/* Open a remote Etherbone device at 'address' (default port 0xEBD0).
 * Negotiation of bus widths is attempted every 3 seconds, 'attempts' times.
 * The proposed_widths is intersected with the remote and local socket widths.
 * From the remaining widths, the largest address and data width is chosen.
 *
 * Return codes:
 *   OK		- the remote etherbone device is ready
 *   ADDRESS	- the network address could not be parsed
 *   TIMEOUT    - timeout waiting for etherbone response
 *   FAIL       - failure of the transport layer (remote host down?)
 *   WIDTH      - could not negotiate an acceptable data bus width
 *   OOM        - out of memory
 */
EB_PUBLIC
eb_status_t eb_device_open(eb_socket_t           socket, 
                           const char*           address,
                           eb_width_t            proposed_widths,
                           int                   attempts,
                           eb_device_t*          result);

/* Open a remote Etherbone device at 'address' (default port 0xEBD0) passively.
 * The channel is opened and the remote device should initiate the EB exchange.
 * This is useful for stream protocols where the master cannot be the initiator.
 *
 * Return codes:
 *   OK		- the remote etherbone device is ready
 *   ADDRESS	- the network address could not be parsed
 *   TIMEOUT    - timeout waiting for etherbone response
 *   FAIL       - failure of the transport layer (remote host down?)
 *   WIDTH      - could not negotiate an acceptable data bus width
 *   OOM        - out of memory
 */
EB_PUBLIC
eb_status_t eb_socket_passive(eb_socket_t           socket, 
                              const char*           address);


/* Recover the negotiated port and address width of the target device.
 */
EB_PUBLIC
eb_width_t eb_device_width(eb_device_t device);

/* Close a remote Etherbone device.
 * Any inflight or ready-to-send cycles will receive EB_TIMEOUT.
 *
 * Return codes:
 *   OK	        - associated memory has been freed
 *   BUSY       - there are unclosed wishbone cycles on this device
 */
EB_PUBLIC
eb_status_t eb_device_close(eb_device_t device);

/* Access the socket backing this device */
EB_PUBLIC
eb_socket_t eb_device_socket(eb_device_t device);

/* Flush all queued cycles to the remote device.
 * Multiple cycles can be packed into a single Etherbone packet.
 * Until this method is called, cycles are only queued, not sent.
 *
 * Return codes:
 *   OK		- queued packets have been sent
 *   FAIL	- the device has a broken link
 */
EB_PUBLIC
eb_status_t eb_device_flush(eb_device_t device);

/* Begin a wishbone cycle on the remote device.
 * Read/write operations within a cycle hold the device locked.
 * Read/write operations are executed in the order they are queued.
 * Until the cycle is closed and device flushed, the operations are not sent.
 *
 * Returns:
 *    FAIL      - device is being closed, cannot create new cycles
 *    OOM       - insufficient memory
 *    OK        - cycle created successfully (your callback will be run)
 * 
 * Your callback will be called exactly once from either:
 *   eb_socket_{run,check} or eb_device_{flush,close}
 * It receives these arguments: cb(user_data, device, operations, status)
 * 
 * If status != OK, the cycle was never sent to the remote bus.
 * If status == OK, the cycle was sent.
 *
 * When status == EB_OK, 'operations' report the wishbone ERR flag.
 * When status != EB_OK, 'operations' points to the offending operation.
 *
 * Callback status codes:
 *   OK		- cycle was executed successfully
 *   ADDRESS    - 1. a specified address exceeded device bus address width
 *                2. the address was not aligned to the operation granularity
 *   WIDTH      - 1. written value exceeded the operation granularity
 *                2. the granularity exceeded the device port width
 *   ENDIAN     - operation format was not word size and no endian was specified
 *   OVERFLOW	- too many operations queued for this cycle (wire limit)
 *   TIMEOUT    - remote system never responded to EB request
 *   FAIL       - remote host violated protocol
 *   OOM        - out of memory while queueing operations to the cycle
 */
EB_PUBLIC
eb_status_t eb_cycle_open(eb_device_t    device, 
                          eb_user_data_t user_data,
                          eb_callback_t  cb,
                          eb_cycle_t*    result);

/* End a wishbone cycle.
 * This places the complete cycle at end of the device's send queue.
 * You will probably want to eb_flush_device soon after calling eb_cycle_close.
 */
EB_PUBLIC
void eb_cycle_close(eb_cycle_t cycle);

/* End a wishbone cycle.
 * This places the complete cycle at end of the device's send queue.
 * You will probably want to eb_flush_device soon after calling eb_cycle_close.
 * This method does NOT check individual wishbone operation error status.
 */
EB_PUBLIC
void eb_cycle_close_silently(eb_cycle_t cycle);

/* End a wishbone cycle.
 * The cycle is discarded, freed, and the callback never invoked.
 */
EB_PUBLIC
void eb_cycle_abort(eb_cycle_t cycle);

/* Access the device targetted by this cycle */
EB_PUBLIC
eb_device_t eb_cycle_device(eb_cycle_t cycle);

/* Prepare a wishbone read operation.
 * The given address is read from the remote device.
 * The result is written to the data address.
 * If data == 0, the result can still be accessed via eb_operation_data.
 *
 * The operation size is max {x in format: x <= data_width(device) }.
 * When the size is not the device data width, format must include an endian.
 * Your address must be aligned to the operation size.
 */
EB_PUBLIC
void eb_cycle_read(eb_cycle_t    cycle, 
                   eb_address_t  address,
                   eb_format_t   format,
                   eb_data_t*    data);
EB_PUBLIC
void eb_cycle_read_config(eb_cycle_t    cycle, 
                          eb_address_t  address,
                          eb_format_t   format,
                          eb_data_t*    data);

/* Perform a wishbone write operation.
 * The given address is written on the remote device.
 * 
 * The operation size is max {x in width: x <= data_width(device) }.
 * When the size is not the device data width, format must include an endian.
 * Your address must be aligned to this operation size and the data must fit.
 */
EB_PUBLIC
void eb_cycle_write(eb_cycle_t    cycle,
                    eb_address_t  address,
                    eb_format_t   format,
                    eb_data_t     data);
EB_PUBLIC
void eb_cycle_write_config(eb_cycle_t    cycle,
                           eb_address_t  address,
                           eb_format_t   format,
                           eb_data_t     data);

/* Operation result accessors */

/* The next operation in the list. EB_NULL = end-of-list */
EB_PUBLIC eb_operation_t eb_operation_next(eb_operation_t op);

/* Was this operation a read? 1=read, 0=write */
EB_PUBLIC int eb_operation_is_read(eb_operation_t op);
/* Was this operation onthe config space? 1=config, 0=wb-bus */
EB_PUBLIC int eb_operation_is_config(eb_operation_t op);
/* Did this operation have an error? 1=error, 0=success */
EB_PUBLIC int eb_operation_had_error(eb_operation_t op);
/* What was the address of this operation? */
EB_PUBLIC eb_address_t eb_operation_address(eb_operation_t op);
/* What was the read or written value of this operation? */
EB_PUBLIC eb_data_t eb_operation_data(eb_operation_t op);
/* What was the format of this operation? */
EB_PUBLIC eb_format_t eb_operation_format(eb_operation_t op);

/* Read the SDB information from the remote bus.
 * If there is not enough memory to initiate the request, EB_OOM is returned.
 * To scan the root bus, Etherbone config space is used to locate the SDB record.
 * When scanning a child bus, supply the bridge's sdb_device record.
 *
 * All fields in the processed structures are in machine native endian.
 * When scanning a child bus, nested addresses are automatically converted.
 *
 * Your callback is called from eb_socket_{run,check} or eb_device_{close,flush}.
 * It receives these arguments: (user_data, device, sdb, status)
 *
 * If status != OK, the SDB information could not be retrieved.
 * If status == OK, the structure was retrieved.
 *
 * The sdb object passed to your callback is only valid until you return.
 * If you need persistent information, you must copy the memory yourself.
 */
typedef void (*sdb_callback_t)(eb_user_data_t, eb_device_t device, sdb_t, eb_status_t);
EB_PUBLIC eb_status_t eb_sdb_scan_bus(eb_device_t device, sdb_bridge_t bridge, eb_user_data_t data, sdb_callback_t cb);
EB_PUBLIC eb_status_t eb_sdb_scan_root(eb_device_t device, eb_user_data_t data, sdb_callback_t cb);

#ifdef __cplusplus
}

/****************************************************************************/
/*                                 C++ API                                  */
/****************************************************************************/

namespace etherbone {

/* Copy the types into the namespace */
typedef eb_address_t address_t;
typedef eb_data_t data_t;
typedef eb_format_t format_t;
typedef eb_status_t status_t;
typedef eb_width_t width_t;
typedef eb_descriptor_t descriptor_t;

class Socket;
class Device;
class Cycle;
class Operation;

class Handler {
  public:
    EB_PUBLIC virtual ~Handler();

    virtual status_t read (address_t address, width_t width, data_t* data) = 0;
    virtual status_t write(address_t address, width_t width, data_t  data) = 0;
};

class Socket {
  public:
    Socket();
    
    status_t open(const char* port = 0, width_t width = EB_DATAX|EB_ADDRX);
    status_t close();
    
    status_t passive(const char* address);
    
    /* attach/detach a virtual device */
    status_t attach(sdb_device_t device, Handler* handler);
    status_t detach(sdb_device_t device);
    
    int run(int timeout_us);
    
    /* These can be used to implement your own 'block': */
    uint32_t timeout() const;
    void descriptors(eb_user_data_t user, eb_descriptor_callback_t list) const;
    void check(uint32_t now, eb_user_data_t user, eb_descriptor_callback_t ready);
    
  protected:
    Socket(eb_socket_t sock);
    eb_socket_t socket;
  
  friend class Device;
};

class Device {
  public:
    Device();
    
    status_t open(Socket socket, const char* address, width_t width = EB_ADDRX|EB_DATAX, int attempts = 5);
    status_t close();
    status_t flush();
    
    const Socket socket() const;
    Socket socket();
    
    width_t width() const;
    
  protected:
    Device(eb_device_t device);
    eb_device_t device;
  
  friend class Cycle;
  template <typename T, void (T::*cb)(Device, Operation, status_t)>
  friend void wrap_member_callback(T* object, eb_device_t dev, eb_operation_t op, eb_status_t status);
  template <typename T, void (*cb)(Device, Operation, status_t)>
  friend void wrap_function_callback(T* user, eb_device_t dev, eb_operation_t op, eb_status_t status);
};

class Cycle {
  public:
    Cycle();
    
    // Start a cycle on the target device.
    template <typename T>
    status_t open(Device device, T* user, void (*cb)(T*, eb_device_t, eb_operation_t, eb_status_t));
    status_t open(Device device);
    
    void abort();
    void close();
    void close_silently();
    
    void read (address_t address, format_t format = EB_DATAX, data_t* data = 0);
    void write(address_t address, format_t format, data_t  data);
    
    void read_config (address_t address, format_t format = EB_DATAX, data_t* data = 0);
    void write_config(address_t address, format_t format, data_t  data);
    
    const Device device() const;
    Device device();
    
  protected:
    Cycle(eb_cycle_t cycle);
    eb_cycle_t cycle;
};

class Operation {
  public:
    bool is_null  () const;
    
    /* Only call these if is_null is false */
    bool is_read  () const;
    bool is_config() const;
    bool had_error() const;
    
    address_t address() const;
    data_t    data   () const;
    format_t  format () const;
    
    const Operation next() const;
    Operation next();
    
  protected:
    Operation(eb_operation_t op);
    
    eb_operation_t operation;

  template <typename T, void (T::*cb)(Device, Operation, status_t)>
  friend void wrap_member_callback(T* object, eb_device_t dev, eb_operation_t op, eb_status_t status);
  template <typename T, void (*cb)(Device, Operation, status_t)>
  friend void wrap_function_callback(T* user, eb_device_t dev, eb_operation_t op, eb_status_t status);
};

/* Convenience templates to convert member functions into callback type */
template <typename T, void (T::*cb)(Device, Operation, status_t)>
void wrap_member_callback(T* object, eb_device_t dev, eb_operation_t op, eb_status_t status) {
  return (object->*cb)(Device(dev), Operation(op), status);
}
template <typename T, void (*cb)(T* user, Device, Operation, status_t)>
void wrap_function_callback(T* user, eb_device_t dev, eb_operation_t op, eb_status_t status) {
  return (*cb)(user, Device(dev), Operation(op), status);
}

/****************************************************************************/
/*                            C++ Implementation                            */
/****************************************************************************/

/* Proxy functions needed by C++ -- ignore these */
EB_PUBLIC eb_status_t eb_proxy_read_handler(eb_user_data_t data, eb_address_t address, eb_width_t width, eb_data_t* ptr);
EB_PUBLIC eb_status_t eb_proxy_write_handler(eb_user_data_t data, eb_address_t address, eb_width_t width, eb_data_t value);

inline Socket::Socket(eb_socket_t sock)
 : socket(sock) { 
}

inline Socket::Socket()
 : socket(EB_NULL) {
}

inline status_t Socket::open(const char* port, width_t width) {
  return eb_socket_open(EB_ABI_CODE, port, width, &socket);
}

inline status_t Socket::close() {
  status_t out = eb_socket_close(socket);
  if (out == EB_OK) socket = EB_NULL;
  return out;
}

inline status_t Socket::passive(const char* address) {
  return eb_socket_passive(socket, address);
}

inline status_t Socket::attach(sdb_device_t device, Handler* handler) {
  struct eb_handler h;
  h.device = device;
  h.data = handler;
  h.read  = &eb_proxy_read_handler;
  h.write = &eb_proxy_write_handler;
  return eb_socket_attach(socket, &h);
}

inline status_t Socket::detach(sdb_device_t device) {
  return eb_socket_detach(socket, device);
}

inline int Socket::run(int timeout_us) {
  return eb_socket_run(socket, timeout_us);
}

inline uint32_t Socket::timeout() const {
  return eb_socket_timeout(socket);
}

inline void Socket::descriptors(eb_user_data_t user, eb_descriptor_callback_t list) const {
  return eb_socket_descriptors(socket, user, list);
}

inline void Socket::check(uint32_t now, eb_user_data_t user, eb_descriptor_callback_t ready) {
  return eb_socket_check(socket, now, user, ready);
}

inline Device::Device(eb_device_t dev)
 : device(dev) {
}

inline Device::Device()
 : device(EB_NULL) { 
}
    
inline status_t Device::open(Socket socket, const char* address, width_t width, int attempts) {
  return eb_device_open(socket.socket, address, width, attempts, &device);
}
    
inline status_t Device::close() {
  status_t out = eb_device_close(device);
  if (out == EB_OK) device = EB_NULL;
  return out;
}

inline const Socket Device::socket() const {
  return Socket(eb_device_socket(device));
}

inline Socket Device::socket() {
  return Socket(eb_device_socket(device));
}

inline width_t Device::width() const {
  return eb_device_width(device);
}

inline status_t Device::flush() {
  return eb_device_flush(device);
}

inline Cycle::Cycle()
 : cycle(EB_NULL) {
}

template <typename T>
inline eb_status_t Cycle::open(Device device, T* user, void (*cb)(T*, eb_device_t, eb_operation_t, status_t)) {
  return eb_cycle_open(device.device, user, reinterpret_cast<eb_callback_t>(cb), &cycle);
}

inline eb_status_t Cycle::open(Device device) {
  return eb_cycle_open(device.device, 0, 0, &cycle);
}

inline void Cycle::abort() {
  eb_cycle_abort(cycle);
  cycle = EB_NULL;
}

inline void Cycle::close() {
  eb_cycle_close(cycle);
  cycle = EB_NULL;
}

inline void Cycle::close_silently() {
  eb_cycle_close_silently(cycle);
  cycle = EB_NULL;
}

inline void Cycle::read(address_t address, format_t format, data_t* data) {
  eb_cycle_read(cycle, address, format, data);
}

inline void Cycle::write(address_t address, format_t format, data_t data) {
  eb_cycle_write(cycle, address, format, data);
}

inline void Cycle::read_config(address_t address, format_t format, data_t* data) {
  eb_cycle_read_config(cycle, address, format, data);
}

inline void Cycle::write_config(address_t address, format_t format, data_t data) {
  eb_cycle_write_config(cycle, address, format, data);
}

inline const Device Cycle::device() const {
  return Device(eb_cycle_device(cycle));
}

inline Device Cycle::device() {
  return Device(eb_cycle_device(cycle));
}

inline Operation::Operation(eb_operation_t op)
 : operation(op) {
}

inline bool Operation::is_null() const {
  return operation == EB_NULL;
}

inline bool Operation::is_read() const {
  return eb_operation_is_read(operation);
}

inline bool Operation::is_config() const {
  return eb_operation_is_config(operation);
}

inline bool Operation::had_error() const {
  return eb_operation_had_error(operation);
}

inline address_t Operation::address() const {
  return eb_operation_address(operation);
}

inline format_t Operation::format() const {
  return eb_operation_format(operation);
}

inline data_t Operation::data() const {
  return eb_operation_data(operation);
}

inline Operation Operation::next() {
  return Operation(eb_operation_next(operation));
}

inline const Operation Operation::next() const {
  return Operation(eb_operation_next(operation));
}

}

#endif

#endif
