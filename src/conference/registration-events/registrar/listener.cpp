#include "listener.hh"
#include "resource-lists.hh"
#include "reginfo.hh"
#include "utils/string-utils.hh"

using namespace std;
using namespace linphone;
using namespace reginfo;
using namespace Xsd::ResourceLists;
using namespace Xsd::XmlSchema;

namespace RegistrationEvent::Registrar {

Listener::Listener(const shared_ptr<linphone::Event> &lev): event(lev) {}

void Listener::onRecordFound(const shared_ptr<Record> &r) {
    this->processRecord(r);
}

void Listener::onContactRegistered(const shared_ptr<Record> &r, const string &uid) {
    this->processRecord(r);
}

void Listener::processRecord(const shared_ptr<Record> &r) {
    list<shared_ptr<ParticipantDeviceIdentity>> compatibleParticipantDevices;

    Reginfo ri = Reginfo(0, State::Value::full);
    Registration re = Registration(
        Uri(this->event->getTo()->asString().c_str()),
        "123",
        Registration::StateType::init
    );
    SofiaAutoHome home;

    if (r) {
        for (const shared_ptr<ExtendedContact> &ec : r->getExtendedContacts()) {
            auto addr = r->getPubGruu(ec, home.home());
            //if (!addr) continue;

            Contact contact = Contact(
                su_sprintf(home.home(), "<%s>", url_as_string(home.home(), addr)),
                Contact::StateType::active,
                Contact::EventType::registered,
                ec->getUniqueId()
            );

            // expires
            if (ec->mSipContact->m_expires) {
                contact.setExpires(atoi(ec->mSipContact->m_expires));
            }

            // unknown-params
            if (ec->mSipContact->m_params) {
                size_t i;

                for (i = 0; ec->mSipContact->m_params[i]; i++) {
                    vector<string> param = StringUtils::split(ec->mSipContact->m_params[i], "=");

                    auto unknownParam = UnknownParam(param.front());
                    if (param.size() == 2) {
                        unknownParam.append(param.back());
                    }

                    contact.getUnknownParam().push_back(unknownParam);
                }
            }

            contact.setDisplayName(this->getDeviceName(ec));
            re.getContact().push_back(contact);

            // If there is some contacts, we set the sate to active
            re.setState(Registration::StateType::active);
        }
    }

    ri.getRegistration().push_back(re);

    stringstream xmlBody;
    serializeReginfo(xmlBody, ri);
    string body = xmlBody.str();

    auto notifyContent = Factory::get()->createContent();
    notifyContent->setBuffer((uint8_t *)body.data(), body.length());
    notifyContent->setType("application");
    notifyContent->setSubtype("reginfo+xml");

    this->event->notify(notifyContent);
};

string Listener::getDeviceName(const shared_ptr<ExtendedContact> &ec) {
    const string &userAgent = ec->getUserAgent();
    size_t begin = userAgent.find("(");
    string deviceName;

    if (begin != string::npos) {
        size_t end = userAgent.find(")", begin);
        size_t openingParenthesis = userAgent.find("(", begin + 1);

        while (openingParenthesis != string::npos && openingParenthesis < end) {
            openingParenthesis = userAgent.find("(", openingParenthesis + 1);
            end = userAgent.find(")", end + 1);
        }

        if (end != string::npos){
            deviceName = userAgent.substr(begin + 1, end - (begin + 1));
        }
    }

    return deviceName;
}

}