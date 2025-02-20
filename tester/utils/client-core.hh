/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010-2023 Belledonne Communications SARL, All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <memory>

#include <linphone++/call_params.hh>
#include <linphone++/linphone.hh>
#include <optional>

#include "asserts.hh"
#include "proxy-server.hh"

namespace flexisip {
namespace tester {

std::shared_ptr<linphone::Core> minimalCore(linphone::Factory& factory);

class Server;
class CoreClient;
class ChatRoomBuilder;
class CallBuilder;
class ClientCall;

/**
 * Class to manage a client Core
 */
class CoreClient {
public:
	/**
	 * @deprecated Use a ClientBuilder
	 */
	CoreClient(const std::string& me, const std::shared_ptr<Server>& server);

	CoreClient(const CoreClient& other) = delete;
	CoreClient(CoreClient&& other) = default;

	~CoreClient();

	const std::shared_ptr<linphone::Core>& getCore() const noexcept {
		return mCore;
	}
	const std::shared_ptr<linphone::Account>& getAccount() const noexcept {
		return mAccount;
	}
	const std::shared_ptr<const linphone::Address>& getMe() const noexcept {
		return mMe;
	}
	std::string getUuid() const {
		return mCore->getConfig()->getString("misc", "uuid", "UNSET!");
	}
	std::string getGruu() const {
		return "\"<urn:uuid:" + getUuid() + ">\"";
	}

	std::chrono::seconds getCallInviteReceivedDelay() const noexcept {
		return mCallInviteReceivedDelay;
	}
	void setCallInviteReceivedDelay(std::chrono::seconds aDelay) noexcept {
		mCallInviteReceivedDelay = aDelay;
	}
	void addListener(const std::shared_ptr<linphone::CoreListener>& listener) const {
		mCore->addListener(listener);
	}

	void disconnect() const;
	void reconnect() const;

	/**
	 * Establish a call
	 *
	 * @param[in] callee 			client to call
	 * @param[in] calleeAddress 	override address of the client to call
	 * @param[in] callerCallParams	call params used by the caller to answer the call. nullptr to use default callParams
	 * @param[in] calleeCallParams	call params used by the callee to accept the call. nullptr to use default callParams
	 *
	 * @return the established call from caller side, nullptr on failure
	 */
	std::shared_ptr<linphone::Call> call(const CoreClient& callee,
	                                     const std::shared_ptr<const linphone::Address>& calleeAddress,
	                                     const std::shared_ptr<linphone::CallParams>& callerCallParams = nullptr,
	                                     const std::shared_ptr<linphone::CallParams>& calleeCallParams = nullptr,
	                                     const std::vector<std::shared_ptr<CoreClient>>& calleeIdleDevices = {});
	std::shared_ptr<linphone::Call> call(const CoreClient& callee,
	                                     const std::shared_ptr<linphone::CallParams>& callerCallParams = nullptr,
	                                     const std::shared_ptr<linphone::CallParams>& calleeCallParams = nullptr,
	                                     const std::vector<std::shared_ptr<CoreClient>>& calleeIdleDevices = {});
	std::shared_ptr<linphone::Call> call(const std::shared_ptr<CoreClient>& callee,
	                                     const std::shared_ptr<linphone::CallParams>& callerCallParams = nullptr,
	                                     const std::shared_ptr<linphone::CallParams>& calleeCallParams = nullptr,
	                                     const std::vector<std::shared_ptr<CoreClient>>& calleeIdleDevices = {});

	/**
	 * Establish a call, but cancel before callee receive it
	 *
	 * @param[in] callee 			client to call
	 * @param[in] callerCallParams	call params used by the caller to answer the call. nullptr to use default callParams
	 * @param[in] calleeCallParams	call params used by the callee to accept the call. nullptr to use default callParams
	 *
	 * @return the established call from caller side, nullptr on failure
	 */
	std::shared_ptr<linphone::Call>
	callWithEarlyCancel(const std::shared_ptr<CoreClient>& callee,
	                    const std::shared_ptr<linphone::CallParams>& callerCallParams = nullptr,
	                    bool isCalleeAway = false);

	/**
	 * Establish a video call.
	 * video is enabled caller side, with or without callParams given
	 *
	 * @param[in] callee 			client to call
	 * @param[in] callerCallParams	call params used by the caller to answer the call. nullptr to use default callParams
	 * @param[in] calleeCallParams	call params used by the callee to accept the call. nullptr to use default callParams
	 *
	 * @return the established call from caller side, nullptr on failure
	 */
	std::shared_ptr<linphone::Call> callVideo(const std::shared_ptr<const CoreClient>& callee,
	                                          const std::shared_ptr<linphone::CallParams>& callerCallParams = nullptr,
	                                          const std::shared_ptr<linphone::CallParams>& calleeCallParams = nullptr);
	std::shared_ptr<linphone::Call> callVideo(const CoreClient& callee,
	                                          const std::shared_ptr<linphone::CallParams>& callerCallParams = nullptr,
	                                          const std::shared_ptr<linphone::CallParams>& calleeCallParams = nullptr);

	/**
	 * Update an ongoing call.
	 * When enable/disable video, check that it is correctly executed on both sides
	 *
	 * @param[in] peer				peer clientCore involved in the call
	 * @param[in] callerCallParams	new call params to be used by self
	 *
	 * @return true if all asserts in the callUpdate succeded, false otherwise
	 */
	bool callUpdate(const CoreClient& peer, const std::shared_ptr<linphone::CallParams>& callerCallParams);

	/**
	 * Get from the two sides the current call and terminate if from this side
	 * assertion failed if one of the client is not in a call or both won't end into Released state
	 *
	 * @param[in]	peer	The other client involved in the call
	 *
	 * @return true if all asserts in the function succeded, false otherwise
	 */
	bool endCurrentCall(const CoreClient& peer);
	bool endCurrentCall(const std::shared_ptr<CoreClient>& peer);

	void runFor(std::chrono::milliseconds duration);

	/**
	 * Iterates the two sides of a fresh call and evaluates whether this end is in
	 * linphone::Call::State::IncomingReceived
	 *
	 * @param[in]	peer	The other client involved in the call
	 *
	 * @return true if there is a current call in IncomingReceived state
	 */
	[[nodiscard]] AssertionResult hasReceivedCallFrom(const CoreClient& peer) const;

	/**
	 * Invites another CoreClient but makes no asserts. Does not iterate any of the Cores.
	 *
	 * @param[in]	peer	The other client to call
	 *
	 * @return the new call. nullptr if the invite failed @maybenil
	 */
	std::shared_ptr<linphone::Call> invite(const CoreClient& peer) const;
	std::shared_ptr<linphone::Call> invite(const CoreClient& peer,
	                                       const std::shared_ptr<const linphone::CallParams>&) const;
	std::shared_ptr<linphone::Call> invite(const std::string&,
	                                       const std::shared_ptr<const linphone::CallParams>& params = nullptr) const;

	std::optional<ClientCall> getCurrentCall() const;
	std::shared_ptr<linphone::CallLog> getCallLog() const;

	// Get listening TCP port. Sets one up at random if not enabled.
	int getTcpPort() const;

	/**
	 * @return The message list for THE FIRST chatroom in the chatroom list
	 */
	std::list<std::shared_ptr<linphone::ChatMessage>> getChatMessages();

	ChatRoomBuilder chatroomBuilder() const;
	CallBuilder callBuilder() const;

private:
	friend class ClientBuilder;

	CoreClient(std::shared_ptr<linphone::Core>&& core,
	           std::shared_ptr<linphone::Account>&& account,
	           std::shared_ptr<const linphone::Address>&& me,
	           const Server& server)
	    : mCore(std::move(core)), mAccount(std::move(account)), mMe(std::move(me)), mServer(server) {
	}

	std::shared_ptr<linphone::Core> mCore;
	std::shared_ptr<linphone::Account> mAccount;
	std::shared_ptr<const linphone::Address> mMe;
	const Server& mServer; /**< Server we're registered to */
	std::chrono::seconds mCallInviteReceivedDelay{5};
}; // class CoreClient

} // namespace tester
} // namespace flexisip
