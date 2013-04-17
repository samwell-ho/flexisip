/*
	Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010  Belledonne Communications SARL.

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

#include <sys/stat.h>
#include <sys/types.h>
#include "eventlogs/eventlogs.hh"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <typeinfo>

using namespace::std;

EventLog::EventLog() {
	su_home_init(&mHome);
	mFrom=NULL;
	mTo=NULL;
	mDate=time(NULL);
	mUA=NULL;
	mCompleted=false;
}

EventLog::~EventLog(){
	su_home_deinit(&mHome);
}

void EventLog::setFrom(const sip_from_t *from){
	mFrom=sip_from_dup(&mHome,from);
}

void EventLog::setTo(const sip_to_t *to){
	mTo=sip_to_dup(&mHome,to);
}

void EventLog::setUserAgent(const sip_user_agent_t *ag){
	mUA=sip_user_agent_dup(&mHome,ag);
}

void EventLog::setStatusCode(int sip_status, const char *reason){
	mStatusCode=sip_status;
	mReason=reason;
}

void EventLog::setCompleted(){
	mCompleted=true;
}

RegistrationLog::RegistrationLog(Type type, const sip_from_t *from, const std::string &instance_id, const sip_contact_t *contacts){
	mType=type;
	setFrom(from);
	mInstanceId=instance_id;
	mContacts=sip_contact_dup(&mHome,contacts);
}


CallLog::CallLog(const sip_from_t *from, const sip_to_t *to){
	setFrom(from);
	setTo(to);
	mStatusCode=0;
	mCancelled=false;
}

void CallLog::setCancelled(){
	mCancelled=true;
}

MessageLog::MessageLog ( MessageLog::ReportType report, const sip_from_t* from, const sip_to_t* to, unsigned long id ) {
	setFrom(from);
	setTo(to);
	mId=id;
	mUri=NULL;
	mReportType=report;
}

void MessageLog::setDestination(const url_t *dest){
	mUri=url_hdup(&mHome,dest);
}

AuthLog::AuthLog(const char *method, const sip_from_t *from, const sip_to_t *to, bool userExists){
	setFrom(from);
	setTo(to);
	mOrigin=NULL;
	mUserExists=userExists;
	mMethod=method;
}

void AuthLog::setOrigin( const sip_via_t* via ) {
	const char *protocol=strchr(via->v_protocol,'/')+1;
	const char *scheme="sip";
	const char *port=via->v_rport ? via->v_rport : via->v_port;
	const char *ip=via->v_received ? via->v_received : via->v_host;
	
	protocol=strchr(protocol,'/')+1;
	
	if (strcasecmp(protocol,"UDP")==0) protocol=NULL;
	else if (strcasecmp(protocol,"UDP")==0) {
		protocol=NULL;
		scheme="sips";
	}
	if (port)
		mOrigin=url_format(&mHome,"%s:%s:%s",scheme,ip,port);
	else 
		mOrigin=url_format(&mHome,"%s:%s",scheme,ip);
	if (protocol)
		mOrigin->url_params=su_sprintf(&mHome,"transport=%s",protocol);
}


static bool createDirectoryIfNotExist(const char *path){
	if (access(path,R_OK|W_OK)==-1){
		if (mkdir(path,S_IRUSR|S_IWUSR|S_IXUSR)==-1){
			LOGE("Cannot create directory %s: %s",path,strerror(errno));
			return false;
		}
	}
	return true;
}


inline ostream & operator<<(ostream & ostr, const sip_user_agent_t *ua){
	char tmp[500]={0};
	sip_user_agent_e(tmp,sizeof(tmp)-1,(msg_header_t*)ua,0);
	ostr<<tmp;
	return ostr;
}

inline ostream & operator<<(ostream & ostr, const url_t *url){
	char tmp[500]={0};
	url_e(tmp,sizeof(tmp)-1,url);
	ostr<<tmp;
	return ostr;
}

inline ostream & operator<<(ostream & ostr, const sip_from_t *from){
	if (from->a_display && *from->a_display!='\0') ostr<<from->a_display;
	ostr<<" <"<<from->a_url<<">";
	return ostr;
}


struct PrettyTime{
	PrettyTime(time_t t) : _t(t){};
	time_t _t;
};

inline ostream & operator<<(ostream & ostr, const PrettyTime &t){
	char tmp[128]={0};
	int len;
	ctime_r(&t._t,tmp);
	len=strlen(tmp);
	if (tmp[len-1]=='\n') tmp[len-1]='\0'; //because ctime_r adds a '\n'
	ostr<<tmp;
	return ostr;
}

inline ostream & operator<<(ostream & ostr, RegistrationLog::Type type){
	switch(type){
		case RegistrationLog::Register:
			ostr<<"Registered";
			break;
		case RegistrationLog::Unregister:
			ostr<<"Unregistered";
			break;
		case RegistrationLog::Expired:
			ostr<<"Registration expired";
			break;
	}
	return ostr;
}

inline ostream &operator<<(ostream & ostr, MessageLog::ReportType type){
	switch(type){
		case MessageLog::Reception:
			ostr<<"Reception";
		break;
		case MessageLog::Delivery:
			ostr<<"Delivery";
		break;
	}
	return ostr;
}

FilesystemEventLogWriter::FilesystemEventLogWriter(const std::string &rootpath) : mRootPath(rootpath), mIsReady(false){
	if (rootpath.c_str()[0]!='/'){
		LOGE("Path for event log writer must be absolute.");
		return;
	}
	if (!createDirectoryIfNotExist(rootpath.c_str()))
		return;
	
	mIsReady=true;
}

bool FilesystemEventLogWriter::isReady()const{
	return mIsReady;
}

int FilesystemEventLogWriter::openPath(const url_t *uri, const char *kind, time_t curtime, int errorcode){
	ostringstream path;
	
	if (errorcode==0){
		const char *username=uri->url_user;
		const char *domain=uri->url_host;
		
		
		path<<mRootPath<<"/users";
		
		if (!createDirectoryIfNotExist(path.str().c_str()))
			return -1;
		
		path<<"/"<<domain;
		
		if (!createDirectoryIfNotExist(path.str().c_str()))
			return -1;
		
		if (!username)
			username="anonymous";
		
		path<<"/"<<username;
		
		if (!createDirectoryIfNotExist(path.str().c_str()))
			return -1;
		path<<"/"<<kind;
	
		if (!createDirectoryIfNotExist(path.str().c_str()))
			return -1;
	}else{
		path<<mRootPath<<"/"<<"errors/";
		if (!createDirectoryIfNotExist(path.str().c_str()))
			return -1;
		path<<kind;
		if (!createDirectoryIfNotExist(path.str().c_str()))
			return -1;
		path<<"/"<<errorcode;
		if (!createDirectoryIfNotExist(path.str().c_str()))
			return -1;
	}
	

	struct tm tm;
	localtime_r(&curtime,&tm);
	path<<"/"<<1900+tm.tm_year<<"-"<<std::setfill('0')<<std::setw(2)<<tm.tm_mon+1<<"-"<<std::setfill('0')<<std::setw(2)<<tm.tm_mday<<".log";
	
	int fd=open(path.str().c_str(),O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
	if (fd==-1){
		LOGE("Cannot open %s: %s",path.str().c_str(),strerror(errno));
		return -1;
	}
	return fd;
}

void FilesystemEventLogWriter::writeRegistrationLog(const std::shared_ptr<RegistrationLog> & rlog){
	const char *label="registers";
	int fd=openPath(rlog->mFrom->a_url,label,rlog->mDate);
	if (fd==-1) return;
	
	ostringstream msg;
	msg<<PrettyTime(rlog->mDate)<<": "<<rlog->mType<<" "<<rlog->mFrom;
	if (rlog->mContacts && rlog->mContacts->m_url) msg<<" ("<<rlog->mContacts->m_url<<") ";
	if (rlog->mUA) msg<<rlog->mUA<<endl;
	
	if (::write(fd,msg.str().c_str(),msg.str().size())==-1){
		LOGE("Fail to write registration log: %s",strerror(errno));
	}
	close(fd);
	if (rlog->mStatusCode>=300){
		writeErrorLog(rlog,label,msg.str());
	}
}

void FilesystemEventLogWriter::writeCallLog(const std::shared_ptr<CallLog> &clog){
	const char *label="calls";
	int fd1=openPath(clog->mFrom->a_url,label,clog->mDate);
	int fd2=openPath(clog->mTo->a_url,label,clog->mDate);
	
	ostringstream msg;
	
	msg<<PrettyTime(clog->mDate)<<": "<<clog->mFrom<<" --> "<<clog->mTo<<" ";
	if (clog->mCancelled) msg<<"Cancelled";
	else msg<<clog->mStatusCode<<" "<<clog->mReason;
	msg<<endl;
	
	if (fd1==-1 || ::write(fd1,msg.str().c_str(),msg.str().size())==-1){
		LOGE("Fail to write registration log: %s",strerror(errno));
	}
	// Avoid to write logs for users that possibly do not exist.
	// However the error will be reported in the errors directory.
	if (clog->mStatusCode!=404){ 
		if (fd2==-1 || ::write(fd2,msg.str().c_str(),msg.str().size())==-1){
			LOGE("Fail to write registration log: %s",strerror(errno));
		}
	}
	if (fd1!=-1) close(fd1);
	if (fd2!=-1) close(fd2);
	if (clog->mStatusCode>=300){
		writeErrorLog(clog,label,msg.str());
	}
}

void FilesystemEventLogWriter::writeMessageLog(const std::shared_ptr<MessageLog> &mlog){
	const char *label="messages";
	int fd=openPath(mlog->mReportType==MessageLog::Reception ? mlog->mFrom->a_url : mlog->mTo->a_url
			,label,mlog->mDate);
	if (fd==-1) return;
	ostringstream msg;
	
	msg<<PrettyTime(mlog->mDate)<<": "<<mlog->mReportType<<" id:"<<std::hex<<mlog->mId<<" "<<std::dec;
	msg<<mlog->mFrom<<" --> "<<mlog->mTo;
	if (mlog->mUri) msg<<" ("<<mlog->mUri<<") ";
	msg<<mlog->mStatusCode<<" "<<mlog->mReason<<endl;
	// Avoid to write logs for users that possibly do not exist.
	// However the error will be reported in the errors directory.
	if (!(mlog->mReportType==MessageLog::Delivery && mlog->mStatusCode==404)){
		if (::write(fd,msg.str().c_str(),msg.str().size())==-1){
			LOGE("Fail to write message log: %s",strerror(errno));
		}
	}
	close(fd);
	if (mlog->mStatusCode>=300){
		writeErrorLog(mlog,label,msg.str());
	}
}

void FilesystemEventLogWriter::writeAuthLog(const std::shared_ptr<AuthLog> &alog){
	const char *label="auth";
	ostringstream msg;
	msg<<PrettyTime(alog->mDate)<<" "<<alog->mMethod<<" "<<alog->mFrom;
	if (alog->mOrigin) msg<<" ("<<alog->mOrigin<<") ";
	if (alog->mUA) msg<<" ("<<alog->mUA<<") ";
	msg<<" --> "<<alog->mTo<<" ";
	msg<<alog->mStatusCode<<" "<<alog->mReason<<endl;
	
	if (alog->mUserExists){
		int fd=openPath(alog->mFrom->a_url,label,alog->mDate);
		if (fd!=-1){
			if (::write(fd,msg.str().c_str(),msg.str().size())==-1){
				LOGE("Fail to write auth log: %s",strerror(errno));
			}
			close(fd);
		}
	}
	writeErrorLog(alog,"auth",msg.str());
}

void FilesystemEventLogWriter::writeErrorLog(const std::shared_ptr<EventLog> &log, const char *kind, const std::string &logstr){
	int fd=openPath(NULL,kind,log->mDate,log->mStatusCode);
	if (fd==-1) return;
	if (::write(fd,logstr.c_str(),logstr.size())==-1){
		LOGE("Fail to write error log: %s",strerror(errno));
	}
	close(fd);
}

void FilesystemEventLogWriter::write(const std::shared_ptr<EventLog> &evlog){
	if (typeid(*evlog.get())==typeid(RegistrationLog)){
		writeRegistrationLog(static_pointer_cast<RegistrationLog>(evlog));
	}else if (typeid(*evlog.get())==typeid(CallLog)){
		writeCallLog(static_pointer_cast<CallLog>(evlog));
	}else if (typeid(*evlog.get())==typeid(MessageLog)){
		writeMessageLog(static_pointer_cast<MessageLog>(evlog));
	}else if (typeid(*evlog.get())==typeid(AuthLog)){
		writeAuthLog(static_pointer_cast<AuthLog>(evlog));
	}
}

