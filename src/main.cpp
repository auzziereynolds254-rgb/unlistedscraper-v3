#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>
#include <Geode/modify/ProfilePage.hpp>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <regex>
#include <set>

#if __has_include(<arc/prelude.hpp>)
	#include <arc/prelude.hpp>
	#define VS_SPAWN(...) ::arc::spawn(__VA_ARGS__)
#else
	#define VS_SPAWN(...) ::geode::async::spawn(__VA_ARGS__)
#endif

using namespace geode::prelude;

namespace {

constexpr char const* kBase   = "https://www.boomlings.com/database/";
constexpr char const* kSecret = "Wmfd2893gb7";

std::string esc(std::string const& s) {
	std::string o;
	o.reserve(s.size() + 16);
	for (char c : s) {
		switch (c) {
			case '"':  o += "\\\""; break;
			case '\\': o += "\\\\"; break;
			case '\n': o += "\\n";  break;
			case '\r': o += "\\r";  break;
			case '\t': o += "\\t";  break;
			default:
				if ((unsigned char)c < 0x20) {
					char b[8];
					snprintf(b, sizeof(b), "\\u%04x", (unsigned char)c);
					o += b;
				} else o += c;
		}
	}
	return o;
}

std::map<int, std::string> parseObj(std::string const& obj) {
	std::map<int, std::string> out;
	auto parts = utils::string::split(obj, ":");
	for (size_t i = 0; i + 1 < parts.size(); i += 2) {
		int k = utils::numFromString<int>(parts[i]).unwrapOr(0);
		if (k > 0) out[k] = parts[i + 1];
	}
	return out;
}

cocos2d::CCNode* findMainLayer(cocos2d::CCNode* root) {
	if (!root) return nullptr;
	if (root->getID() == "main-layer") return root;
	if (auto arr = root->getChildren()) {
		for (unsigned i = 0; i < arr->count(); ++i) {
			if (auto f = findMainLayer(static_cast<cocos2d::CCNode*>(arr->objectAtIndex(i)))) return f;
		}
	}
	return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------

class ScraperNode : public cocos2d::CCNode {
public:
	struct LevelRec {
		int id;
		std::string raw;
		std::string src;
	};

	static ScraperNode* create(int accountID, int userID, std::string const& username) {
		auto ret = new ScraperNode();
		if (ret && ret->init(accountID, userID, username)) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}

	bool init(int accountID, int userID, std::string const& username) {
		if (!CCNode::init()) return false;
		m_accountID = accountID;
		m_userID    = userID;
		m_username  = username;
		m_delay     = Mod::get()->getSettingValue<int>("delay") / 1000.f;
		m_range     = Mod::get()->getSettingValue<int>("probe-range");
		m_maxProbes = Mod::get()->getSettingValue<int>("max-probes");
		m_maxPages  = Mod::get()->getSettingValue<int>("max-pages");

		m_label = cocos2d::CCLabelBMFont::create("", "bigFont.fnt");
		m_label->setScale(0.42f);
		m_label->setAnchorPoint({0.5f, 1.f});
		m_label->setPosition({0.f, -18.f});
		m_label->setOpacity(220);
		this->addChild(m_label);

		this->scheduleUpdate();
		return true;
	}

	bool isFinished() const { return m_finished; }

	void startNow() {
		if (m_running || m_finished) return;
		m_startDelay = 0.f;
	}

	void restart() {
		m_finished   = false;
		m_running    = true;
		m_inFlight   = false;
		m_startDelay = 0.f;
		m_cooldown   = 0.f;
		m_phase      = 0;
		m_queueIdx   = 0;
		m_probeIdx   = 0;
		m_probeCount = 0;
		m_requests   = 0;
		m_queue.clear();
		m_probeList.clear();
		m_levels.clear();
		m_rawBlocks.clear();
		m_commentIds.clear();
		m_userInfoRaw.clear();
		m_seen.clear();
		buildQueue();
		updateLabel();
	}

	void update(float dt) override {
		if (m_finished) return;

		if (m_startDelay > 0.f) {
			m_startDelay -= dt;
			if (m_startDelay <= 0.f) {
				m_running  = true;
				m_cooldown = 0.f;
				buildQueue();
			}
			return;
		}
		if (!m_running || m_inFlight) return;

		m_cooldown -= dt;
		if (m_cooldown > 0.f) return;
		m_cooldown = m_delay;

		if (m_phase == 0) {
			if (m_queueIdx >= m_queue.size()) { beginProbing(); return; }
			auto task = m_queue[m_queueIdx++];
			fire(task.first, task.second);
			return;
		}

		if (m_phase == 1) {
			if (m_probeIdx >= m_probeList.size()) { finish(); return; }
			int id = m_probeList[m_probeIdx++];
			m_probeCount++;
			fire("getGJLevels21", {
				{"type", "0"}, {"str", std::to_string(id)}, {"page", "0"}, {"total", "0"}
			});
			updateLabel();
		}
	}

protected:
	using Params = std::vector<std::pair<std::string, std::string>>;

	int m_accountID = 0;
	int m_userID    = 0;
	std::string m_username;

	float m_startDelay = 0.65f;
	float m_cooldown   = 0.f;
	float m_delay      = 0.25f;
	int   m_range      = 8;
	int   m_maxProbes  = 400;
	int   m_maxPages   = 10;
	bool  m_running    = false;
	bool  m_finished   = false;
	bool  m_inFlight   = false;
	int   m_phase      = 0;

	size_t m_queueIdx = 0;
	size_t m_probeIdx = 0;
	int    m_probeCount = 0;
	int    m_requests   = 0;

	std::vector<std::pair<std::string, Params>> m_queue;
	std::vector<int> m_probeList;
	std::vector<LevelRec> m_levels;
	std::vector<std::string> m_rawBlocks;
	std::set<int> m_commentIds;
	std::set<int> m_seen;
	std::string m_userInfoRaw;
	cocos2d::CCLabelBMFont* m_label = nullptr;

	void buildQueue() {
		if (m_userID <= 0) { finish(); return; }

		m_queue.push_back({"getGJUserInfo20", {
			{"targetAccountID", std::to_string(m_accountID)}
		}});

		for (int p = 0; p < m_maxPages; ++p) {
			m_queue.push_back({"getGJLevels21", {
				{"type", "5"}, {"str", std::to_string(m_userID)},
				{"page", std::to_string(p)}, {"total", "0"}
			}});
		}

		if (!m_username.empty()) {
			m_queue.push_back({"getGJLevels21", {
				{"type", "0"}, {"str", m_username}, {"page", "0"}, {"total", "0"}
			}});
		}

		if (Mod::get()->getSettingValue<bool>("scan-comments")) {
			for (int p = 0; p < 3; ++p) {
				m_queue.push_back({"getGJComments21", {
					{"userID", std::to_string(m_accountID)},
					{"page", std::to_string(p)}, {"mode", "0"}, {"total", "0"}
				}});
			}
		}
		updateLabel();
	}

	void enqueueAround(int center, int radius) {
		for (int d = 1; d <= radius; ++d) {
			for (int id : { center - d, center + d }) {
				if (id <= 0) continue;
				if (m_seen.count(id)) continue;
				if ((int)m_probeList.size() >= m_maxProbes) return;
				m_seen.insert(id);
				m_probeList.push_back(id);
			}
		}
	}

	void beginProbing() {
		m_phase = 1;
		for (auto const& l : m_levels) {
			m_seen.insert(l.id);
			enqueueAround(l.id, m_range);
		}
		for (int id : m_commentIds) {
			if (m_seen.count(id)) continue;
			if ((int)m_probeList.size() >= m_maxProbes) break;
			m_seen.insert(id);
			m_probeList.push_back(id);
		}
		if (m_probeList.empty()) { finish(); return; }
		updateLabel();
	}

	void fire(std::string const& ep, Params params) {
		m_inFlight = true;
		m_requests++;

		auto req = web::WebRequest();
		req.header("User-Agent", "");
		req.timeout(20);
		req.param("gameVersion", "22");
		req.param("binaryVersion", "42");
		req.param("gdw", "0");
		req.param("secret", kSecret);
		for (auto const& [k, v] : params) req.param(k, v);

		std::string url = std::string(kBase) + ep;
		this->retain();

		VS_SPAWN(req.post(url), [this, ep](web::WebResponse res) {
			m_inFlight = false;
			std::string body;
			if (res.ok()) body = res.string().unwrapOr(std::string());
			if (body == "-1" || body == "-2" || body == "error code: 1005") body.clear();

			if (!body.empty()) {
				if (ep == "getGJLevels21") {
					handleLevels(body);
				} else if (ep == "getGJUserInfo20") {
					m_userInfoRaw = body;
				} else if (ep == "getGJComments21") {
					handleComments(body);
				}
				m_rawBlocks.push_back("=== " + ep + " ===\n" + body);
			}
			updateLabel();
			this->release();
		});
	}

	void handleLevels(std::string const& body) {
		auto parts = utils::string::split(body, "#");
		for (size_t i = 1; i < parts.size(); ++i) {
			if (parts[i].empty() || parts[i] == "-1") continue;
			auto f = parseObj(parts[i]);
			auto uidIt = f.find(6);
			auto lidIt = f.find(1);
			if (uidIt == f.end() || lidIt == f.end()) continue;
			if (utils::numFromString<int>(uidIt->second).unwrapOr(0) != m_userID) continue;
			int lid = utils::numFromString<int>(lidIt->second).unwrapOr(0);
			if (lid <= 0) continue;

			bool dup = false;
			for (auto const& l : m_levels) if (l.id == lid) { dup = true; break; }
			if (dup) continue;

			m_levels.push_back({ lid, parts[i], m_phase == 0 ? "list" : "probe" });
			if (m_phase == 1) {
				m_seen.insert(lid);
				enqueueAround(lid, std::min(m_range, 3));
			}
		}
	}

	void handleComments(std::string const& body) {
		static std::regex const re("\\b(\\d{5,9})\\b");
		int added = 0;
		for (auto it = std::sregex_iterator(body.begin(), body.end(), re);
			 it != std::sregex_iterator() && added < 200; ++it) {
			int id = utils::numFromString<int>(it->str()).unwrapOr(0);
			if (id > 0 && !m_commentIds.count(id)) { m_commentIds.insert(id); ++added; }
		}
	}

	void updateLabel() {
		if (!m_label) return;
		if (m_finished) return;
		if (m_startDelay > 0.f && !m_running) {
			m_label->setString("preparing...");
			return;
		}
		m_label->setString(fmt::format("{} req / {} probes / {} found",
			m_requests, m_probeCount, m_levels.size()).c_str());
	}

	std::string buildJson() {
		std::string j = "{\n";
		j += fmt::format("  \"target\": {{\"accountID\": {}, \"userID\": {}, \"username\": \"{}\"}},\n",
			m_accountID, m_userID, esc(m_username));
		j += fmt::format("  \"stats\": {{\"requests\": {}, \"probes\": {}, \"levelsFound\": {}}},\n",
			m_requests, m_probeCount, m_levels.size());
		j += "  \"note\": \"source=list came from a list/search endpoint; source=probe was found by ID probing and is likely unlisted. Raw field keys are GD's own; key 68 is the unlisted flag on servers that send it.\",\n";
		j += "  \"userInfoRaw\": \"" + esc(m_userInfoRaw) + "\",\n";
		j += "  \"levels\": [\n";

		for (size_t i = 0; i < m_levels.size(); ++i) {
			auto const& L = m_levels[i];
			auto f = parseObj(L.raw);
			std::string name;
			if (auto n = f.find(2); n != f.end()) name = utils::string::urlDecode(n->second);

			j += "    {\"id\": " + std::to_string(L.id);
			j += ", \"source\": \"" + esc(L.src) + "\"";
			j += ", \"name\": \"" + esc(name) + "\"";
			j += ", \"fields\": {";
			bool first = true;
			for (auto const& [k, v] : f) {
				if (!first) j += ", ";
				first = false;
				j += "\"" + std::to_string(k) + "\": \"" + esc(v) + "\"";
			}
			j += "}, \"raw\": \"" + esc(L.raw) + "\"}";
			if (i + 1 < m_levels.size()) j += ",";
			j += "\n";
		}

		j += "  ],\n  \"rawResponses\": [\n";
		for (size_t i = 0; i < m_rawBlocks.size(); ++i) {
			j += "    \"" + esc(m_rawBlocks[i]) + "\"";
			if (i + 1 < m_rawBlocks.size()) j += ",";
			j += "\n";
		}
		j += "  ]\n}\n";
		return j;
	}

	void finish() {
		if (m_finished) return;
		m_finished = true;
		m_running  = false;

		auto json = buildJson();
		auto dir  = Mod::get()->getSaveDir() / "scans";
		utils::file::createDirectoryAll(dir);
		auto path = dir / fmt::format("{}.json", m_userID);
		bool saved = utils::file::writeString(path, json).isOk();
		utils::clipboard::write(json);

		if (m_label) m_label->setString(fmt::format("done: {} levels", m_levels.size()).c_str());

		std::string msg = fmt::format(
			"{} level objects, {} requests.\n\nJSON copied to clipboard.{}\n\n{}",
			m_levels.size(), m_requests,
			saved ? "\nSaved to:\n" + path.string() : "\nFile save failed.",
			m_levels.empty() ? "No levels returned - the user may have no public uploads." : "");

		if (auto alert = FLAlertLayer::create("Scan Complete", msg, "OK")) alert->show();
	}
};

// ---------------------------------------------------------------------------

class $modify(VSProfilePage, ProfilePage) {
	struct Fields {
		ScraperNode* scraper = nullptr;
	};

	bool init(GJUserScore* score) {
		if (!ProfilePage::init(score)) return false;
		if (!score) return true;

		int acc = score->getAccountID();
		int uid = score->getUserID();
		if (uid <= 0) return true;

		auto host = findMainLayer(this);
		if (!host) host = this;

		auto spr  = ButtonSprite::create("SCAN", "goldFont.fnt", "GJ_button_01.png", 0.62f);
		auto item = cocos2d::CCMenuItemSpriteExtra::create(
			spr, this, menu_selector(VSProfilePage::onScan));

		auto menu = cocos2d::CCMenu::create();
		menu->setID("scraper-menu"_spr);
		menu->addChild(item);
		auto sz = host->getContentSize();
		menu->setPosition({ sz.width - 56.f, sz.height - 34.f });
		host->addChild(menu, 60);

		auto node = ScraperNode::create(acc, uid, score->getUserName());
		node->setID("scraper-node"_spr);
		node->setPosition({ sz.width - 56.f, sz.height - 34.f });
		this->addChild(node, 61);
		m_fields->scraper = node;

		if (Mod::get()->getSettingValue<bool>("auto-scan")) node->startNow();
		return true;
	}

	void onScan(cocos2d::CCObject*) {
		auto node = m_fields->scraper;
		if (!node) return;
		if (node->isFinished()) node->restart();
		else node->startNow();
	}
};
