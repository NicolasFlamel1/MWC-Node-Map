// Header files
#include <arpa/inet.h>
#include <filesystem>
#include "git2.h"
#include <ifaddrs.h>
#include <iostream>
#include "maxminddb.h"
#include <memory>
#include <net/if.h>
#include "./node/mwc_validation_node.h"
#include <regex>
#include <termios.h>

using namespace std;


// Constants

// IP geolocate database location
static const char *IP_GEOLOCATE_DATABASE_LOCATION = "./ip_geolocate_database.mmdb";

// Recent peers JSON location
static const char *RECENT_PEERS_JSON_LOCATION = "./peers.json";

// Git repo refspecs
static const char *GIT_REPO_REFSPECS = "refs/heads/master";

// Git uploader name
static const char *GIT_UPLOADER_NAME = TOSTRING(PROGRAM_NAME) " Automatic Updater";

// Tor SOCKS proxy address
static const char *TOR_SOCKS_PROXY_ADDRESS = "localhost";

// Tor SOCKS proxy port
static const uint16_t TOR_SOCKS_PROXY_PORT = 9050;

// Check if floonet
#ifdef ENABLE_FLOONET

	// Listening port
	static const uint16_t LISTENING_PORT = 9031;
	
// Otherwise
#else

	// Listening port
	static const uint16_t LISTENING_PORT = 9030;
#endif

// Upload recent peers JSON file interval
static const chrono::hours UPLOAD_RECENT_PEERS_JSON_FILE_INTERVAL = 168h;

// Min longitude
static const double MIN_LONGITUDE = -180;

// Max longitude
static const double MAX_LONGITUDE = 180;

// Min latitude
static const double MIN_LATITUDE = -90;

// Max latitude
static const double MAX_LATITUDE = 90;

// Known user agent pattern
static const regex KNOWN_USER_AGENT_PATTERN(R"(^(?:MW\/MWC|MWC Validation Node|MWC Pay|MWC Node Map) \d{1,3}\.\d{1,3}\.\d{1,3}$)");


// Structures

// Geolocation structure
struct Geolocation {

	// Continent
	string continent;
	
	// Country
	string country;
	
	// Subdivision
	string subdivision;
	
	// City
	string city;
	
	// Longitude
	double longitude = NAN;
	
	// Latitude
	double latitude = NAN;
};


// Function prototypes

// Geolocate
static Geolocation geolocate(const string &address);

// Upload recent peers JSON file
static void uploadRecentPeersJsonFile(const char *accessToken);


// Main function
int main() {

	// Try
	try {
	
		// Intialize access token
		string accessToken;
		
		// Automatically securely clear access token when done
		const unique_ptr<string, void(*)(string *)> accessTokenUniquePointer(&accessToken, [](string *accessToken) {
		
			// Securely clear access token
			explicit_bzero(accessToken->data(), accessToken->size());
		});
		
		// Display message
		cout << "Enter Git access token to use when uploading recent peers JSON file: ";
		
		// Check if getting input settings failed
		termios savedInputSettings;
		if(tcgetattr(STDIN_FILENO, &savedInputSettings)) {
		
			// Display message
			cout << endl << "Getting input settings failed" << endl;
			
			// Return failure
			return EXIT_FAILURE;
		}
		
		// Check if silencing echo in input settings failed
		termios newInputSettings = savedInputSettings;
		newInputSettings.c_lflag &= ~ECHO;
		if(tcsetattr(STDIN_FILENO, TCSANOW, &newInputSettings)) {
		
			// Restore input settings
			tcsetattr(STDIN_FILENO, TCSANOW, &savedInputSettings);
			
			// Display message
			cout << endl << "Silencing echo in input settings failed" << endl;
			
			// Return failure
			return EXIT_FAILURE;
		}
		
		// Check if getting access token failed
		if(!getline(cin, accessToken)) {
		
			// Restore input settings
			tcsetattr(STDIN_FILENO, TCSANOW, &savedInputSettings);
			
			// Display message
			cout << endl << "Getting access token failed" << endl;
			
			// Return failure
			return EXIT_FAILURE;
		}
		
		// Check if restoring input settings failed
		if(tcsetattr(STDIN_FILENO, TCSANOW, &savedInputSettings)) {
		
			// Restore input settings
			tcsetattr(STDIN_FILENO, TCSANOW, &savedInputSettings);
			
			// Display message
			cout << endl << "Restoring input settings failed" << endl;
			
			// Return failure
			return EXIT_FAILURE;
		}
		
		// Check if access token exists
		if(!accessToken.empty()) {
		
			// Display message
			cout << endl << "Using provided access token to upload recent peers JSON file at set intervals" << endl;
		}
		
		// Otherwise
		else {
		
			// Display message
			cout << endl << "No access token provided. Never uploading recent peers JSON file" << endl;
		}
		
		// Try
		try {
		
			// Check if deleting recent peers JSON file failed
			if(filesystem::exists(RECENT_PEERS_JSON_LOCATION) && !filesystem::remove(RECENT_PEERS_JSON_LOCATION)) {
			
				// Display message
				cout << "Deleting recent peers JSON file failed" << endl;
				
				// Return failure
				return EXIT_FAILURE;
			}
		}
		
		// Catch errors
		catch(...) {
		
			// Display message
			cout << "Deleting recent peers JSON file failed" << endl;
			
			// Return failure
			return EXIT_FAILURE;
		}
		
		// Create node
		MwcValidationNode::Node node;
		
		// Initialize recent peers JSON file lock
		mutex recentPeersJsonFileLock;
		
		// Set node's on peer info callback
		node.setOnPeerInfoCallback([&recentPeersJsonFileLock](MwcValidationNode::Node &node, const string &peerIdentifier, const MwcValidationNode::Node::Capabilities capabilities, const string &userAgent, const uint32_t protocolVersion, const uint64_t baseFee, const uint64_t totalDifficulty, const bool isInbound) -> void {
		
			// Try
			try {
			
				// Geolocate the peer
				const Geolocation geolocation = geolocate(peerIdentifier);
				
				// Lock recent peers JSON file
				lock_guard lock(recentPeersJsonFileLock);
				
				// Get if recent peers JSON file exists
				const bool fileExists = filesystem::exists(RECENT_PEERS_JSON_LOCATION);
				
				// Set recent peers JSON file to throw an exception on error
				ofstream fout;
				fout.exceptions(ios::badbit | ios::failbit);
				
				// Open recent peers JSON file
				fout.open(RECENT_PEERS_JSON_LOCATION, ios::binary | ios::app);
				
				// Append peer's info to recent peers JSON file
				fout << (fileExists ? ',' : '[') << endl << "{"
				
					// Address
					"\"address\":" << quoted(peerIdentifier.ends_with(".onion") ? to_string(hash<string>{}(peerIdentifier)) + ".onion" : peerIdentifier) << ","
					
					// Capabilities
					"\"capabilities\":\"" << static_cast<uint32_t>(capabilities) << "\","
					
					// User agent
					"\"user_agent\":" << quoted(regex_match(userAgent, KNOWN_USER_AGENT_PATTERN) ? userAgent : "Unknown") << ","
					
					// Base fee
					"\"base_fee\":\"" << baseFee << "\","
					
					// Continent
					"\"continent\":";
					
					// Check if geolocation's continent doesn't exist
					if(geolocation.continent.empty()) {
					
						// Append no continent to recent peers JSON file
						fout << "null";
					}
					
					// Otherwise
					else {
					
						// Append continent to recent peers JSON file
						fout << quoted(geolocation.continent);
					}
					
					// Country
					fout << ",\"country\":";
					
					// Check if geolocation's country doesn't exist
					if(geolocation.country.empty()) {
					
						// Append no country to recent peers JSON file
						fout << "null";
					}
					
					// Otherwise
					else {
					
						// Append country to recent peers JSON file
						fout << quoted(geolocation.country);
					}
					
					// Subdivision
					fout << ",\"subdivision\":";
					
					// Check if geolocation's subdivision doesn't exist
					if(geolocation.subdivision.empty()) {
					
						// Append no subdivision to recent peers JSON file
						fout << "null";
					}
					
					// Otherwise
					else {
					
						// Append subdivision to recent peers JSON file
						fout << quoted(geolocation.subdivision);
					}
					
					// City
					fout << ",\"city\":";
					
					// Check if geolocation's city doesn't exist
					if(geolocation.city.empty()) {
					
						// Append no city to recent peers JSON file
						fout << "null";
					}
					
					// Otherwise
					else {
					
						// Append city to recent peers JSON file
						fout << quoted(geolocation.city);
					}
					
					// Longitude
					fout << ",\"longitude\":" << (!isnan(geolocation.longitude) ? '"' + to_string(geolocation.longitude) + '"' : "null") << ","
					
					// Latitude
					"\"latitude\":" << (!isnan(geolocation.latitude) ? '"' + to_string(geolocation.latitude) + '"' : "null") <<
				"}";
				
				// Close recent peers JSON file
				fout.close();
			}
			
			// Catch errors
			catch(const exception &error) {
			
				// Display message
				cout << "Updating recent peers JSON file failed: " << error.what() << endl;
				
				// Return
				return;
			}
			
			// Catch errors
			catch(...) {
			
				// Display message
				cout << "Updating recent peers JSON file failed" << endl;
				
				// Return
				return;
			}
			
			// Display message
			cout << "Detected " << (isInbound ? "inbound" : "outbound") << " peer " << peerIdentifier << endl;
		});
		
		// Set node's on peer healthy callback
		node.setOnPeerHealthyCallback([](MwcValidationNode::Node &node, const string &peerIdentifier) -> bool {
		
			// Return false to disconnect from peer
			return false;
		});
		
		// Check if getting network interface addresses failed
		ifaddrs *networkInterfaceAddresses;
		if(getifaddrs(&networkInterfaceAddresses)) {
			
			// Display message
			cout << "Getting network interface addresses failed" << endl;
			
			// Return failure
			return EXIT_FAILURE;
		}
		
		// Automatically free network interface addresses when done
		const unique_ptr<ifaddrs, decltype(&freeifaddrs)> networkInterfaceAddressesUniquePointer(networkInterfaceAddresses, freeifaddrs);
		
		// Check if floonet
		#ifdef ENABLE_FLOONET
		
			// Display message
			cout << "Node will connect to the floonet network" << endl;
			
		// Otherwise
		#else
		
			// Display message
			cout << "Node will connect to the mainnet network" << endl;
		#endif
		
		// Check if Tor is enabled
		#ifdef ENABLE_TOR
		
			// Check if Tor SOCKS proxy address is an IPv6 address
			in6_addr temp;
			if(inet_pton(AF_INET6, TOR_SOCKS_PROXY_ADDRESS, &temp) == 1) {
			
				// Display message
				cout << "Node will use the Tor SOCKS proxy at [" << TOR_SOCKS_PROXY_ADDRESS << "]:" << TOR_SOCKS_PROXY_PORT << endl;
			}
			
			// Otherwise
			else {
			
				// Display message
				cout << "Node will use the Tor SOCKS proxy at " << TOR_SOCKS_PROXY_ADDRESS << ':' << TOR_SOCKS_PROXY_PORT << endl;
			}
		#endif
		
		// Go through all network interface addresses
		bool networkInterfaceFound = false;
		for(const ifaddrs *networkInterfaceAddress = networkInterfaceAddresses; networkInterfaceAddress; networkInterfaceAddress = networkInterfaceAddress->ifa_next) {
		
			// Check if network interface isn't lookback and its address exists
			if(!(networkInterfaceAddress->ifa_flags & IFF_LOOPBACK) && networkInterfaceAddress->ifa_addr) {
			
				// Check if network interface address is an IPv4 address
				if(networkInterfaceAddress->ifa_addr->sa_family == AF_INET) {
				
					// Check if getting the network interface address's IP address was successful
					char ipAddress[INET_ADDRSTRLEN];
					if(inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in *>(networkInterfaceAddress->ifa_addr)->sin_addr, ipAddress, sizeof(ipAddress))) {
					
						// Display message
						cout << "Node will listen at " << ipAddress << ':' << LISTENING_PORT << endl;
						
						// Start node listening on the IP address
						node.start(TOR_SOCKS_PROXY_ADDRESS, TOR_SOCKS_PROXY_PORT, nullptr, MwcValidationNode::Node::DEFAULT_BASE_FEE, ipAddress, LISTENING_PORT, MwcValidationNode::Node::Capabilities::NONE);
						
						// Set network interface found to true
						networkInterfaceFound = true;
						
						// Break
						break;
					}
				}
				
				// Otherwise check if network interface address is an IPv6 address
				else if(networkInterfaceAddress->ifa_addr->sa_family == AF_INET6) {
				
					// Check if getting the network interface address's IP address was successful
					char ipAddress[INET6_ADDRSTRLEN];
					if(inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6 *>(networkInterfaceAddress->ifa_addr)->sin6_addr, ipAddress, sizeof(ipAddress))) {
					
						// Display message
						cout << "Node will listen at [" << ipAddress << "]:" << LISTENING_PORT << endl;
						
						// Start node listening on the IP address
						node.start(TOR_SOCKS_PROXY_ADDRESS, TOR_SOCKS_PROXY_PORT, nullptr, MwcValidationNode::Node::DEFAULT_BASE_FEE, ipAddress, LISTENING_PORT, MwcValidationNode::Node::Capabilities::NONE);
						
						// Set network interface found to true
						networkInterfaceFound = true;
						
						// Break
						break;
					}
				}
			}
		}
		
		// Check if no network interface was found
		if(!networkInterfaceFound) {
		
			// Display message
			cout << "No network interface found for the node to listen at" << endl;
			
			// Return failure
			return EXIT_FAILURE;
		}
		
		// Set last upload recent peers JSON file time to now
		chrono::time_point lastUploadRecentPeersJsonFileTime = chrono::steady_clock::now();
		
		// Loop while not closing
		while(!MwcValidationNode::Common::isClosing()) {
		
			// Check if access token exists and time to upload peers
			if(!accessToken.empty() && chrono::steady_clock::now() - lastUploadRecentPeersJsonFileTime >= UPLOAD_RECENT_PEERS_JSON_FILE_INTERVAL) {
			
				// Set error occurred to false
				bool errorOccurred = false;
				
				// Try
				try {
				
					// Lock recent peers JSON file
					lock_guard lock(recentPeersJsonFileLock);
					
					// Try
					try {
					
						// Get if recent peers JSON file exists
						const bool fileExists = filesystem::exists(RECENT_PEERS_JSON_LOCATION);
						
						// Set recent peers JSON file to throw an exception on error
						ofstream fout;
						fout.exceptions(ios::badbit | ios::failbit);
						
						// Open recent peers JSON file
						fout.open(RECENT_PEERS_JSON_LOCATION, ios::binary | ios::app);
						
						// Append end of peers to recent peers JSON file
						fout << (fileExists ? "" : "[") << endl << ']';
						
						// Close recent peers JSON file
						fout.close();
						
						// Upload recent peers JSON file
						uploadRecentPeersJsonFile(accessToken.c_str());
					}
					
					// Catch errors
					catch(const exception &error) {
					
						// Display message
						cout << "Uploading recent peers JSON file failed: " << error.what() << endl;
						
						// Set error occurred to true
						errorOccurred = true;
					}
					
					// Catch errors
					catch(...) {
					
						// Display message
						cout << "Uploading recent peers JSON file failed" << endl;
						
						// Set error occurred to true
						errorOccurred = true;
					}
					
					// Try
					try {
					
						// Check if deleting recent peers JSON file failed
						if(filesystem::exists(RECENT_PEERS_JSON_LOCATION) && !filesystem::remove(RECENT_PEERS_JSON_LOCATION)) {
						
							// Display message
							cout << "Deleting recent peers JSON file failed" << endl;
							
							// Return failure
							return EXIT_FAILURE;
						}
					}
					
					// Catch errors
					catch(...) {
					
						// Display message
						cout << "Deleting recent peers JSON file failed" << endl;
						
						// Return failure
						return EXIT_FAILURE;
					}
				}
				
				// Catch errors
				catch(...) {
				
					// Display message
					cout << "Uploading recent peers JSON file failed" << endl;
					
					// Set error occurred to true
					errorOccurred = true;
				}
				
				// Check if an error didn't occur
				if(!errorOccurred) {
				
					// Display message
					cout << "Successfully uploading recent peers JSON file" << endl;
				}
				
				// Set last upload recent peers JSON file time to now
				lastUploadRecentPeersJsonFileTime = chrono::steady_clock::now();
			}
			
			// Sleep
			this_thread::sleep_for(1s);
		}
	}
	
	// Catch errors
	catch(const exception &error) {
	
		// Display message
		cout << "Unexpected error occurred: " << error.what() << endl;
		
		// Return failure
		return EXIT_FAILURE;
	}
	
	// Catch errors
	catch(...) {
	
		// Display message
		cout << "Unexpected error occurred" << endl;
		
		// Return failure
		return EXIT_FAILURE;
	}
	
	// Return failure is an error occurred otherwise return success
	return MwcValidationNode::Common::errorOccurred() ? EXIT_FAILURE : EXIT_SUCCESS;
}


// Supporting function implementation

// Geolocate
Geolocation geolocate(const string &address) {

	// Initialize geolocation
	Geolocation geolocation;
	
	// Initialize IP address
	sockaddr_storage ipAddress;
	
	// Check if address is an IPv6 address and port
	if(address.starts_with('[') && address.contains(']')) {
	
		// Get address without port
		const string addressWithoutPort = address.substr(sizeof('['), address.find(']') - sizeof('['));
		
		// Check if parsing the address without port as an IPv6 address was successful
		if(inet_pton(AF_INET6, addressWithoutPort.c_str(), &reinterpret_cast<sockaddr_in6 *>(&ipAddress)->sin6_addr) == 1) {
		
			// Set IP address's family to IPv6
			ipAddress.ss_family = AF_INET6;
		}
		
		// Otherwise
		else {
		
			// Return geolocation
			return geolocation;
		}
	}
	
	// Otherwise check if address is an IPv4 address and port
	else if(address.contains(':')) {
	
		// Get address without port
		const string addressWithoutPort = address.substr(0, address.find(':'));
		
		// Check if parsing the address without port as an IPv4 address was successful
		if(inet_pton(AF_INET, addressWithoutPort.c_str(), &reinterpret_cast<sockaddr_in *>(&ipAddress)->sin_addr) == 1) {
		
			// Set IP address's family to IPv4
			ipAddress.ss_family = AF_INET;
		}
		
		// Otherwise
		else {
		
			// Return geolocation
			return geolocation;
		}
	}
	
	// Otherwise
	else {
	
		// Return geolocation
		return geolocation;
	}
	
	// Check if opening the IP geolocate database failed
	MMDB_s ipGeolocateDatabase;
	if(MMDB_open(IP_GEOLOCATE_DATABASE_LOCATION, MMDB_MODE_MMAP, &ipGeolocateDatabase) != MMDB_SUCCESS) {
	
		// Throw exception
		throw runtime_error("Opening the IP geolocate database failed");
	}
	
	// Automatically close the IP geolocate database when done
	const unique_ptr<MMDB_s, decltype(&MMDB_close)> addressInfoUniquePointer(&ipGeolocateDatabase, MMDB_close);
	
	// Check if looking up the IP address in the IP geolocate database failed
	int error;
	MMDB_lookup_result_s ipGeolocateResult = MMDB_lookup_sockaddr(&ipGeolocateDatabase, reinterpret_cast<const sockaddr *>(&ipAddress), &error);
	if(error != MMDB_SUCCESS) {
	
		// Throw exception
		throw runtime_error("Looking up the IP address in the IP geolocate database failed");
	}
	
	// Check if IP address doesn't exist in the IP geolocate database
	if(!ipGeolocateResult.found_entry) {
	
		// Return geolocation
		return geolocation;
	}
	
	// Check if getting the IP geolocate result's continent was successful
	MMDB_entry_data_s ipGeolocateEntryData;
	if(MMDB_get_value(&ipGeolocateResult.entry, &ipGeolocateEntryData, "continent", "names", "en", nullptr) == MMDB_SUCCESS && ipGeolocateEntryData.has_data && ipGeolocateEntryData.type == MMDB_DATA_TYPE_UTF8_STRING && MwcValidationNode::Common::isUtf8(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.data_size)) {
	
		// Set geolocation's continent to the result
		geolocation.continent = string(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.utf8_string + ipGeolocateEntryData.data_size);
	}
	
	// Check if getting the IP geolocate result's country was successful
	if(MMDB_get_value(&ipGeolocateResult.entry, &ipGeolocateEntryData, "country", "names", "en", nullptr) == MMDB_SUCCESS && ipGeolocateEntryData.has_data && ipGeolocateEntryData.type == MMDB_DATA_TYPE_UTF8_STRING && MwcValidationNode::Common::isUtf8(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.data_size)) {
	
		// Set geolocation's country to the result
		geolocation.country = string(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.utf8_string + ipGeolocateEntryData.data_size);
	}
	
	// Check if getting the IP geolocate result's subdivision was successful
	if(MMDB_get_value(&ipGeolocateResult.entry, &ipGeolocateEntryData, "subdivisions", "0", "names", "en", nullptr) == MMDB_SUCCESS && ipGeolocateEntryData.has_data && ipGeolocateEntryData.type == MMDB_DATA_TYPE_UTF8_STRING && MwcValidationNode::Common::isUtf8(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.data_size)) {
	
		// Set geolocation's subdivision to the result
		geolocation.subdivision = string(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.utf8_string + ipGeolocateEntryData.data_size);
	}
	
	// Check if getting the IP geolocate result's city was successful
	if(MMDB_get_value(&ipGeolocateResult.entry, &ipGeolocateEntryData, "city", "names", "en", nullptr) == MMDB_SUCCESS && ipGeolocateEntryData.has_data && ipGeolocateEntryData.type == MMDB_DATA_TYPE_UTF8_STRING && MwcValidationNode::Common::isUtf8(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.data_size)) {
	
		// Set geolocation's city to the result
		geolocation.city = string(ipGeolocateEntryData.utf8_string, ipGeolocateEntryData.utf8_string + ipGeolocateEntryData.data_size);
	}
	
	// Check if getting the IP geolocate result's longitude was successful
	if(MMDB_get_value(&ipGeolocateResult.entry, &ipGeolocateEntryData, "location", "longitude", nullptr) == MMDB_SUCCESS && ipGeolocateEntryData.has_data && ipGeolocateEntryData.type == MMDB_DATA_TYPE_DOUBLE && isfinite(ipGeolocateEntryData.double_value) && ipGeolocateEntryData.double_value >= MIN_LONGITUDE && ipGeolocateEntryData.double_value <= MAX_LONGITUDE) {
	
		// Set geolocation's longitude to the result
		geolocation.longitude = ipGeolocateEntryData.double_value;
		
		// Check if getting the IP geolocate result's latitude failed
		if(MMDB_get_value(&ipGeolocateResult.entry, &ipGeolocateEntryData, "location", "latitude", nullptr) == MMDB_SUCCESS && ipGeolocateEntryData.has_data && ipGeolocateEntryData.type == MMDB_DATA_TYPE_DOUBLE && isfinite(ipGeolocateEntryData.double_value) && ipGeolocateEntryData.double_value >= MIN_LATITUDE && ipGeolocateEntryData.double_value <= MAX_LATITUDE) {
		
			// Set geolocation's latitude to the result
			geolocation.latitude = ipGeolocateEntryData.double_value;
		}
		
		// Otherwise
		else {
		
			// Reset geolocation's longitude
			geolocation.longitude = NAN;
		}
	}
	
	// Return geolocation
	return geolocation;
}

// Upload recent peers JSON file
void uploadRecentPeersJsonFile(const char *accessToken) {

	// Check if initializing Git failed
	const int initializeGitResult = git_libgit2_init();
	if(initializeGitResult < 0) {
	
		// Throw exception
		throw runtime_error("Initializing Git failed");
	}
	
	// Automatically shutdown Git when done
	const unique_ptr<int, void(*)(int *)> initializeGitResultUniquePointer(const_cast<int *>(&initializeGitResult), [](int *initializeGitResultPointer) {
	
		// Shutdown Git
		git_libgit2_shutdown();
	});
	
	// Check if opening repo failed
	git_repository *repo;
	if(git_repository_open(&repo, "./") < 0) {
	
		// Throw exception
		throw runtime_error("Opening repo failed");
	}
	
	// Automatically free repo when done
	const unique_ptr<git_repository, decltype(&git_repository_free)> repoUniquePointer(repo, git_repository_free);
	
	// Check if getting repo's index failed
	git_index *index;
	if(git_repository_index(&index, repo) < 0) {
	
		// Throw exception
		throw runtime_error("Getting repo's index failed");
	}
	
	// Automatically free index when done
	const unique_ptr<git_index, decltype(&git_index_free)> indexUniquePointer(index, git_index_free);
	
	// Check if changing index to update recent peers JSON file failed
	if(git_index_add_bypath(index, &RECENT_PEERS_JSON_LOCATION[sizeof("./") - sizeof('\0')]) < 0) {
	
		// Throw exception
		throw runtime_error("Changing index to update recent peers JSON file failed");
	}
	
	// Check if saving index failed
	if(git_index_write(index) < 0) {
	
		// Throw exception
		throw runtime_error("Saving index failed");
	}
	
	// Check if getting tree ID from the index failed
	git_oid treeId;
	if(git_index_write_tree(&treeId, index) < 0) {
	
		// Throw exception
		throw runtime_error("Getting tree ID from the index failed");
	}
	
	// Check if getting tree with the tree ID failed
	git_tree *tree;
	if(git_tree_lookup(&tree, repo, &treeId) < 0) {
	
		// Throw exception
		throw runtime_error("Getting tree with the tree ID failed");
	}
	
	// Automatically free tree when done
	const unique_ptr<git_tree, decltype(&git_tree_free)> treeUniquePointer(tree, git_tree_free);
	
	// Check if creating signature failed
	git_signature *signature;
	if(git_signature_now(&signature, GIT_UPLOADER_NAME, "unknown") < 0) {
	
		// Throw exception
		throw runtime_error("Creating signature failed");
	}
	
	// Automatically free signature when done
	const unique_ptr<git_signature, decltype(&git_signature_free)> signatureUniquePointer(signature, git_signature_free);
	
	// Check if getting repo's head ID failed
	git_oid headId;
	if(git_reference_name_to_id(&headId, repo, "HEAD") < 0) {
	
		// Throw exception
		throw runtime_error("Getting repo's head ID failed");
	}
	
	// Check if getting head commit failed
	git_commit *headCommit;
	if(git_commit_lookup(&headCommit, repo, &headId) < 0) {
	
		// Throw exception
		throw runtime_error("Getting head commit failed");
	}
	
	// Automatically free head commit when done
	const unique_ptr<git_commit, decltype(&git_commit_free)> headCommitUniquePointer(headCommit, git_commit_free);
	
	// Check if creating commit for the tree failed
	git_oid commitId;
	if(git_commit_create(&commitId, repo, "HEAD", signature, signature, "UTF-8", (string("Automatically updated ") + &RECENT_PEERS_JSON_LOCATION[sizeof("./") - sizeof('\0')]).c_str(), tree, 1, const_cast<const git_commit **>(&headCommit)) < 0) {
	
		// Throw exception
		throw runtime_error("Creating commit for the tree failed");
	}
	
	// Check if getting repo's remote failed
	git_remote *remote;
	if(git_remote_lookup(&remote, repo, "origin") < 0) {
	
		// Throw exception
		throw runtime_error("Getting repo's remote failed");
	}
	
	// Automatically free remote when done
	const unique_ptr<git_remote, decltype(&git_remote_free)> remoteUniquePointer(remote, git_remote_free);
	
	// Set refspecs
	const git_strarray refspecs = {
	
		// Strings
		const_cast<char **>(&GIT_REPO_REFSPECS),
		
		// Count
		1
	};
	
	// Set push options
	git_push_options pushOptions = GIT_PUSH_OPTIONS_INIT;
	pushOptions.callbacks.payload = const_cast<char *>(accessToken);
	pushOptions.callbacks.credentials = [](git_credential **out, const char *url, const char *usernameFromUrl, unsigned int allowedTypes, void *payload) -> int {
	
		// Check if plain text credentials is allowed
		if(allowedTypes & GIT_CREDENTIAL_USERPASS_PLAINTEXT) {
		
			// Get access token from payload
			const char *accessToken = reinterpret_cast<const char *>(payload);
			
			// Return plain text credentials
			return git_credential_userpass_plaintext_new(out, GIT_UPLOADER_NAME, accessToken);
		}
		
		// Otherwise
		else {
		
			// Return error
			return -1;
		}
	};
	
	// Check if pushing changes to remote failed
	if(git_remote_push(remote, &refspecs, &pushOptions) < 0) {
	
		// Throw exception
		throw runtime_error("Pushing changes to remote failed");
	}
}
