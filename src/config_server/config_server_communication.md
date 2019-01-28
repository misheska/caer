The configuration server is accessible through a TCP connection, with optional TLS v1.2 encryption and authentication support.
Clients connect to the server and can then send commands and receive data back.

All communication to and from the configuration server is done using [Flatbuffers](https://google.github.io/flatbuffers/index.html), a schema-based serialization protocol originally developed by Google.
The following schema is used:

`
namespace dv;

enum ConfigAction : int8 {
	ERROR              = 0,
	NODE_EXISTS        = 1,
	ATTR_EXISTS        = 2,
	GET_CHILDREN       = 3,
	GET_ATTRIBUTES     = 4,
	GET_TYPE           = 5,
	GET_RANGES         = 6,
	GET_FLAGS          = 7,
	GET_DESCRIPTION    = 8,
	GET                = 9,
	PUT                = 10,
	ADD_MODULE         = 11,
	REMOVE_MODULE      = 12,
	ADD_PUSH_CLIENT    = 13,
	REMOVE_PUSH_CLIENT = 14,
	PUSH_MESSAGE_NODE  = 15,
	PUSH_MESSAGE_ATTR  = 16,
	DUMP_TREE          = 17,
	DUMP_TREE_NODE     = 18,
	DUMP_TREE_ATTR     = 19,
	GET_CLIENT_ID      = 20
}

enum ConfigType : int8 {
	UNKNOWN = -1,
	BOOL    = 0,
	INT     = 3,
	LONG    = 4,
	FLOAT   = 5,
	DOUBLE  = 6,
	STRING  = 7
}

enum ConfigNodeEvents : int8 {
	NODE_ADDED   = 0,
	NODE_REMOVED = 1
}

enum ConfigAttributeEvents : int8 {
	ATTRIBUTE_ADDED    = 0,
	ATTRIBUTE_MODIFIED = 1,
	ATTRIBUTE_REMOVED  = 2
}

table ConfigActionData {
	action: ConfigAction = ERROR;
	nodeEvents: ConfigNodeEvents; // Node related.
	attrEvents: ConfigAttributeEvents; // Attribute related.
	id: uint64;
	node: string;
	key: string;
	type: ConfigType = UNKNOWN;
	value: string;
	// On attribute creation only.
	ranges: string;
	flags: int32;
	description: string;
}

root_type ConfigActionData;
`

The buffer content is defined in ConfigActionData. The 'action' member is the only one that's required to be always set. The ConfigType, ConfigNodeEvents and ConfigAttributeEvents enums reflect the values found in the main dv::Config namespace, adapted for the Flatbuffer schema here. The ConfigAction enum defines all possible action that the configuration server can execute or respond with.
The following table goes into detail of what data the client needs to send or what data he can expect back, based on the ConfigAction sent. On success, the response message will have the same action value, with possibly other additional data members set (see 'Server to Client Response'); on failure, ERROR is returned with a string explaining the problem.

ConfigAction | Client to Server Message | Server to Client Response
-------------|--------------------------|--------------------------
ERROR | N/A (only from server due to errors). | Sent whenever an error occours. Error message is contained in member 'value'.
NODE_EXISTS | Check if a given 'node' exists. | Member 'value' either "true" or "false".
ATTR_EXISTS | Check if a given 'key' exists for the given 'node'. | Member 'value' either "true" or "false".
GET_CHILDREN | Get the names of all children of a 'node'. | Member 'value' contains all child node names, separated by '\|'.
GET_ATTRIBUTES | Get the names of all attribute keys of a 'node'. | Member 'value' contains all attribute key names, separated by '\|'.
GET_TYPE | Get the type of the 'key' of 'node'. | Member 'type' contains key type.
GET_RANGES | Get the min/max ranges for the 'key' with 'type' of 'node'. | Member 'ranges' contains the minimum and maximun range as strings, separated by '\|'.
GET_FLAGS | Get the flags set for the 'key' with 'type' of 'node'. | Member 'flags' contains the current flag state.
GET_DESCRIPTION | Get the description (help text) for the 'key' with 'type' of 'node'. | Member 'description' contains the text as string.
GET | Get the current value for the 'key' with 'type' of 'node'. | Member 'value' contains the current value, formatted as a string.
PUT | Update the value for the 'key' with 'type' of 'node', using the string member 'value'. | No additional return value.
ADD_MODULE | Create a new module with name of 'node' and library of 'key'. | No additional return value.
REMOVE_MODULE | Remove the module with name of 'node'. Only works with mainloop stopped. | No additional return value.
ADD_PUSH_CLIENT | Add this client to the list of clients that shall receive push notifications on configuration changes. No input values. | No additional return value.
REMOVE_PUSH_CLIENT | Remove this client from the list of clients that shall receive push notifications on configuration changes. No input values. | No additional return value.
PUSH_MESSAGE_NODE | N/A (only from server after ADD_PUSH_CLIENT). | Detected change on a node. 'id' contains the ID of the change originator (0=runtime, 1-N=connected client), 'nodeEvents' the kind of change, 'node' the path of the node that changed.
PUSH_MESSAGE_ATTR | N/A (only from server after ADD_PUSH_CLIENT). | Detected change on an attribute. 'id' contains the ID of the change originator (0=runtime, 1-N=connected client), 'attrEvents' the kind of change, 'node' the path of the node where the attribute resides, 'key' the name of the attribute key, 'type' the type of the attribute, 'value' the new value. On attribute addition (attrEvents=ATTRIBUTE_ADDED), the following members are also set: 'flags' to the current flags value, 'ranges' to the min/max ranges and 'description' to the current description (help text).
DUMP_TREE | Send a full dump of the current configuration tree. No input values. | No additional return value. Confirmation means dump is complete.
DUMP_TREE_NODE | N/A (only from server in response to DUMP_TREE). | Next node encountered in full configuration dump. Contains node path in member 'node'.
DUMP_TREE_ATTR | N/A (only from server in response to DUMP_TREE). | Next attribute encountered in full configuration dump. Contains node path in member 'node', attribute key name in 'key', key type in 'type', current value in 'value', current flags in 'flags', min/max ranges in 'ranges' and description (help text) in 'description'.
GET_CLIENT_ID | Get the ID of this client. No input values. | Member 'id' contains the 64bit numeric ID of this client.
