// DUO-038 content reviewed and corrected by Codex; see documentation/duo/reviews/DUO-038.md.
#include <cmath>
#include <stdexcept>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "../src/game_state.h"
#include "../src/object_interactions.h"

namespace {
int g_checks = 0;
int g_failures = 0;
void Fail(int line, const std::string& message) {
    ++g_failures;
    std::printf("FAIL line %d: %s\n", line, message.c_str());
}
void ExpectTrue(bool value, const char* expression, int line) {
    ++g_checks;
    if (!value) Fail(line, std::string("expected true: ") + expression);
}
void ExpectFalse(bool value, const char* expression, int line) {
    ++g_checks;
    if (value) Fail(line, std::string("expected false: ") + expression);
}
void ExpectString(const std::string& actual, const std::string& expected, int line) {
    ++g_checks;
    if (actual != expected) Fail(line, "got \"" + actual + "\", expected \"" + expected + "\"");
}
#define EXPECT_TRUE(expression) ExpectTrue((expression), #expression, __LINE__)
#define EXPECT_FALSE(expression) ExpectFalse((expression), #expression, __LINE__)
#define EXPECT_STRING(actual, expected) ExpectString((actual), (expected), __LINE__)

const char* const kPlaceA = "canonical:quarry_threshold";
const char* const kPlaceB = "generated:0002";
const char* const kChalk = "boundary chalk";
const char* const kMap = "survey map slate";
const char* const kTongs = "sample tongs";
const char* const kReference = "Un petit caillou repose ici. Un tas de gravats repose ici. Un mur ferme la pièce.";
const char* const kPebbleQuote = "Un petit caillou repose ici.";
const char* const kRubbleQuote = "Un tas de gravats repose ici.";
const char* const kWallQuote = "Un mur ferme la pièce.";

liminal::SessionState MakeFixture() {
    liminal::SessionState state;
    state.hard_state = liminal::MakeInitialHardState();
    state.current_place_id = kPlaceA;
    liminal::EnsureObjectRegistry(&state);
    return state;
}
bool Contains(const std::vector<std::string>& values, const std::string& value) {
    for (const std::string& entry : values) if (entry == value) return true;
    return false;
}
bool InstanceMatches(const liminal::ObjectInstance& instance, const std::string& item) {
    if (instance.id == item || instance.label == item || instance.visual_id == item) return true;
    for (const std::string& alias : instance.aliases) if (alias == item) return true;
    return false;
}
std::string FindInstanceId(const liminal::SessionState& state, const std::string& item, int line) {
    for (const auto& entry : state.objects.instances)
        if (InstanceMatches(entry.second, item)) return entry.first;
    Fail(line, "no object instance for item \"" + item + "\"");
    return std::string();
}
std::string NewInstanceId(const liminal::ObjectState& before, const liminal::ObjectState& after,
                          int line) {
    for (const auto& entry : after.instances)
        if (before.instances.count(entry.first) == 0) return entry.first;
    Fail(line, "no instance was created");
    return std::string();
}
const std::vector<liminal::ObjectPlacement>* DepositsAt(const liminal::SessionState& state,
                                                        const std::string& place) {
    const auto it = state.objects.deposits.find(place);
    return it == state.objects.deposits.end() ? nullptr : &it->second;
}
bool HasConsumedSource(const liminal::ObjectState& objects, const std::string& object_id) {
    for (const liminal::ObjectSource& source : objects.sources)
        if (source.object_id == object_id && source.remaining == 0) return true;
    return false;
}
struct Snapshot {
    std::string objects;
    std::vector<std::string> inventory;
    explicit Snapshot(const liminal::SessionState& s)
        : objects(liminal::SerializeObjectState(s.objects)), inventory(s.hard_state.inventory_items) {}
};
void ExpectSameState(const liminal::SessionState& s, const Snapshot& before, int line) {
    ++g_checks;
    if (liminal::SerializeObjectState(s.objects) != before.objects) Fail(line, "object state was mutated");
    ++g_checks;
    if (s.hard_state.inventory_items != before.inventory) Fail(line, "inventory was mutated");
}
void ExpectValid(const liminal::SessionState& state, int line) {
    std::string error;
    ++g_checks;
    if (!liminal::ValidateObjectState(state, &error)) Fail(line, "expected valid state: " + error);
}
void ExpectInvalid(const liminal::SessionState& state, int line) {
    std::string error;
    ++g_checks;
    if (liminal::ValidateObjectState(state, &error) || error.empty())
        Fail(line, "expected invalid state with an error");
}
liminal::ObjectPlacement PlacementAt(const std::string& object_id, float x, float y, float z) {
    liminal::ObjectPlacement p;
    p.object_id = object_id; p.x = x; p.y = y; p.z = z;
    return p;
}
liminal::ObjectPlacement DropPlacement(const std::string& object_id) {
    return PlacementAt(object_id, 0.0f, 0.0f, 1.0f);
}
liminal::ObjectProposal ExplicitProposal(const char* action, const std::string& id, int revision) {
    liminal::ObjectProposal p;
    p.action = action; p.origin = "explicit"; p.object_id = id;
    p.verdict = "allow"; p.revision = revision; p.durable = false;
    return p;
}
liminal::ObjectProposal DropProposal(const std::string& id, int revision) {
    return ExplicitProposal("drop", id, revision);
}
liminal::ObjectProposal TakeProposal(const std::string& id, int revision) {
    return ExplicitProposal("take", id, revision);
}
liminal::ObjectProposal ImplicitTakeProposal(const std::string& quote, const std::string& label,
                                             int revision, bool material_source) {
    liminal::ObjectProposal p;
    p.action = "take"; p.origin = "implicit"; p.quote = quote; p.label = label;
    p.verdict = "allow"; p.revision = revision; p.durable = false;
    p.material_source = material_source;
    return p;
}
bool ExpectAccepted(const liminal::SessionState& base, const liminal::ObjectProposal& proposal,
                    const liminal::ObjectPlacement* placement,
                    liminal::SessionState* candidate, int line) {
    const Snapshot before(base);
    liminal::SessionState prepared = base;
    std::string reason;
    ++g_checks;
    if (!liminal::PrepareObjectTransfer(base, proposal, placement, &prepared, &reason)) {
        Fail(line, "expected accepted transfer: " + reason);
        return false;
    }
    ++g_checks;
    if (prepared.objects.revision != base.objects.revision + 1)
        Fail(line, "successful transfer did not increment revision");
    ExpectSameState(base, before, line);
    *candidate = prepared;
    return true;
}
void ExpectRefused(const liminal::SessionState& base, const liminal::ObjectProposal& proposal,
                   const liminal::ObjectPlacement* placement, int line) {
    liminal::SessionState candidate = base;
    const Snapshot candidate_before(candidate);
    const Snapshot source_before(base);
    std::string reason;
    const bool accepted =
        liminal::PrepareObjectTransfer(base, proposal, placement, &candidate, &reason);
    ++g_checks;
    if (accepted) Fail(line, "expected a refused transfer");
    else if (reason.empty()) Fail(line, "refused transfer without reason");
    ExpectSameState(candidate, candidate_before, line);
    ExpectSameState(base, source_before, line);
}
#define EXPECT_ACCEPTED(base, proposal, placement, candidate) \
    ExpectAccepted((base), (proposal), (placement), (candidate), __LINE__)
#define EXPECT_REFUSED(base, proposal, placement) \
    ExpectRefused((base), (proposal), (placement), __LINE__)
#define EXPECT_SAME_STATE(state, snapshot) ExpectSameState((state), (snapshot), __LINE__)
#define EXPECT_VALID(state) ExpectValid((state), __LINE__)
#define EXPECT_INVALID(state) ExpectInvalid((state), __LINE__)
#define FIND_INSTANCE(state, item) FindInstanceId((state), (item), __LINE__)
#define NEW_INSTANCE(before, after) NewInstanceId((before), (after), __LINE__)

void TestInitialRegistry() {
    liminal::SessionState s = MakeFixture();
    EXPECT_TRUE(s.hard_state.inventory_items.size() == 3 && s.objects.instances.size() == 3);
    EXPECT_VALID(s);
    for (const char* tool : {kChalk, kMap, kTongs}) {
        EXPECT_TRUE(s.objects.instances.count(tool)==1 && Contains(s.hard_state.inventory_items,tool));
        const auto it = s.objects.instances.find(FIND_INSTANCE(s, tool));
        EXPECT_TRUE(it != s.objects.instances.end() && it->second.portable);
    }
    const Snapshot before(s); liminal::EnsureObjectRegistry(&s);
    EXPECT_SAME_STATE(s, before);
}

void TestExplicitDrop() {
    liminal::SessionState s = MakeFixture();
    const auto before = s.hard_state.inventory_items;
    const std::string id = FIND_INSTANCE(s, kChalk);
    const auto p = DropPlacement(id);
    liminal::SessionState c;
    if (!EXPECT_ACCEPTED(s, DropProposal(id, s.objects.revision), &p, &c)) return;
    EXPECT_TRUE(c.hard_state.inventory_items.size() == before.size() - 1);
    EXPECT_FALSE(Contains(c.hard_state.inventory_items, kChalk));
    const auto* d = DepositsAt(c, kPlaceA);
    EXPECT_TRUE(d != nullptr && d->size() == 1 && (*d)[0].object_id == id);
    EXPECT_TRUE(d != nullptr && !d->empty() && (*d)[0].x == 0.0f && (*d)[0].y == 0.0f && (*d)[0].z == 1.0f);
    EXPECT_VALID(c);
}

void TestTravelAndReturn() {
    liminal::SessionState s = MakeFixture();
    const std::string id = FIND_INSTANCE(s, kChalk);
    const auto p = DropPlacement(id);
    liminal::SessionState dropped;
    if (!EXPECT_ACCEPTED(s, DropProposal(id, s.objects.revision), &p, &dropped)) return;
    liminal::SessionState away = dropped; away.current_place_id = kPlaceB;
    EXPECT_REFUSED(away, TakeProposal(id, away.objects.revision), nullptr);
    liminal::SessionState back = dropped; back.current_place_id = kPlaceA;
    back.hard_state.turn_number += 20;
    back.history.clear();
    liminal::SessionState taken;
    if (!EXPECT_ACCEPTED(back, TakeProposal(id, back.objects.revision), nullptr, &taken)) return;
    EXPECT_TRUE(Contains(taken.hard_state.inventory_items, kChalk));
    EXPECT_TRUE(taken.objects.instances.count(id) == 1);
    const auto* d = DepositsAt(taken, kPlaceA);
    EXPECT_TRUE(d == nullptr || d->empty());
}

void TestDoubleMovesAndStaleRevision() {
    liminal::SessionState s = MakeFixture();
    const std::string id = FIND_INSTANCE(s, kChalk);
    const auto p = DropPlacement(id);
    liminal::SessionState d1;
    if (!EXPECT_ACCEPTED(s, DropProposal(id, s.objects.revision), &p, &d1)) return;
    liminal::SessionState t1;
    if (!EXPECT_ACCEPTED(d1, TakeProposal(id, d1.objects.revision), nullptr, &t1)) return;
    EXPECT_REFUSED(t1, TakeProposal(id, t1.objects.revision), nullptr);
    liminal::SessionState d2;
    if (!EXPECT_ACCEPTED(s, DropProposal(id, s.objects.revision), &p, &d2)) return;
    EXPECT_REFUSED(d2, DropProposal(id, d2.objects.revision), &p);
    EXPECT_REFUSED(d1, TakeProposal(id, s.objects.revision), nullptr);
}

void TestAllThreeObjects() {
    liminal::SessionState s = MakeFixture();
    const auto items = s.hard_state.inventory_items;
    std::vector<std::string> ids;
    for (const std::string& item : items) {
        const std::string id = FIND_INSTANCE(s, item);
        if (id.empty()) return;
        ids.push_back(id);
        const auto p = DropPlacement(id);
        liminal::SessionState next;
        if (!EXPECT_ACCEPTED(s, DropProposal(id, s.objects.revision), &p, &next)) return;
        s = next;
    }
    EXPECT_TRUE(s.hard_state.inventory_items.empty());
    const auto* d = DepositsAt(s, kPlaceA);
    EXPECT_TRUE(d != nullptr && d->size() == 3);
    for (const std::string& id : ids) {
        liminal::SessionState next;
        if (!EXPECT_ACCEPTED(s, TakeProposal(id, s.objects.revision), nullptr, &next)) return;
        s = next;
    }
    EXPECT_TRUE(s.hard_state.inventory_items.size() == 3);
    for (const std::string& item : items) EXPECT_TRUE(Contains(s.hard_state.inventory_items, item));
    liminal::SessionState fresh = MakeFixture();
    EXPECT_TRUE(fresh.hard_state.inventory_items.size() == 3 && fresh.objects.instances.size() == 3);
    EXPECT_VALID(fresh);
}

void TestInvalidDropRequests() {
    liminal::SessionState s = MakeFixture();
    const std::string id = FIND_INSTANCE(s, kChalk);
    auto p = DropPlacement(id);
    EXPECT_REFUSED(s, DropProposal(id, s.objects.revision), nullptr);
    p.y = std::numeric_limits<float>::infinity();
    EXPECT_REFUSED(s, DropProposal(id, s.objects.revision), &p);
    const auto unknown = DropPlacement("missing-object");
    EXPECT_REFUSED(s, DropProposal("missing-object", s.objects.revision), &unknown);
    EXPECT_REFUSED(s, TakeProposal("missing-object", s.objects.revision), nullptr);
}

void TestImplicitTakeAndDepositInB() {
    liminal::SessionState s = MakeFixture();
    s.reference_descriptions[kPlaceA] = kReference;
    liminal::SessionState taken;
    if (!EXPECT_ACCEPTED(s, ImplicitTakeProposal(kPebbleQuote, "caillou", s.objects.revision, false), nullptr, &taken)) return;
    const std::string id = NEW_INSTANCE(s.objects, taken.objects);
    if (id.empty()) return;
    EXPECT_TRUE(taken.hard_state.inventory_items.size() == s.hard_state.inventory_items.size() + 1);
    EXPECT_TRUE(Contains(taken.hard_state.inventory_items, id));
    EXPECT_TRUE(HasConsumedSource(taken.objects, id));
    EXPECT_REFUSED(taken, ImplicitTakeProposal(kPebbleQuote, "petit caillou", taken.objects.revision, false), nullptr);
    liminal::SessionState at_b = taken; at_b.current_place_id = kPlaceB;
    const auto p = DropPlacement(id);
    liminal::SessionState dropped;
    if (!EXPECT_ACCEPTED(at_b, DropProposal(id, at_b.objects.revision), &p, &dropped)) return;
    liminal::SessionState retaken;
    if (!EXPECT_ACCEPTED(dropped, TakeProposal(id, dropped.objects.revision), nullptr, &retaken)) return;
    EXPECT_TRUE(Contains(retaken.hard_state.inventory_items, id));
    const auto* d = DepositsAt(retaken, kPlaceB);
    EXPECT_TRUE(d == nullptr || d->empty());
    liminal::SessionState back = retaken; back.current_place_id = kPlaceA;
    EXPECT_REFUSED(back, ImplicitTakeProposal(kPebbleQuote, "caillou", back.objects.revision, false), nullptr);
}

void TestMaterialSourceAndInterdictions() {
    liminal::SessionState s = MakeFixture();
    s.reference_descriptions[kPlaceA] = kReference;
    liminal::SessionState rubble;
    if (!EXPECT_ACCEPTED(s, ImplicitTakeProposal(kRubbleQuote, "gravats", s.objects.revision, true), nullptr, &rubble)) return;
    const std::string id = NEW_INSTANCE(s.objects, rubble.objects);
    if (id.empty()) return;
    EXPECT_TRUE(HasConsumedSource(rubble.objects, id));
    EXPECT_REFUSED(rubble, ImplicitTakeProposal(kRubbleQuote, "gravats", rubble.objects.revision, true), nullptr);
    EXPECT_REFUSED(s, ImplicitTakeProposal(kWallQuote, "mur", s.objects.revision, false), nullptr);
    EXPECT_REFUSED(s, ImplicitTakeProposal("Un rocher absent.", "rocher", s.objects.revision, false), nullptr);
    auto uncertain = ImplicitTakeProposal(kPebbleQuote, "caillou", s.objects.revision, false);
    uncertain.verdict = "uncertain";
    EXPECT_REFUSED(s, uncertain, nullptr);
}

void AddLocalObject(liminal::SessionState& s, const std::string& id, const std::string& place,
                    float x) {
    liminal::ObjectInstance o;
    o.id = id; o.label = "caillou"; o.origin_place = place; o.portable = true;
    o.aliases.push_back("caillou");
    s.objects.instances[id] = o;
    s.objects.deposits[place].push_back(PlacementAt(id, x, 0.0f, 1.0f));
}

void TestInvalidPartitions() {
    liminal::SessionState s = MakeFixture();
    liminal::SessionState duplicate = s;
    duplicate.hard_state.inventory_items.push_back(kChalk);
    EXPECT_INVALID(duplicate);
    const std::string id = FIND_INSTANCE(s, kChalk);
    const auto p = DropPlacement(id); liminal::SessionState dropped;
    if (!EXPECT_ACCEPTED(s, DropProposal(id, s.objects.revision), &p, &dropped)) return;
    liminal::SessionState two_places = dropped;
    two_places.objects.deposits[kPlaceB].push_back(PlacementAt(id, 1.0f, 0.0f, 1.0f));
    EXPECT_INVALID(two_places);
    liminal::SessionState non_finite = dropped;
    non_finite.objects.deposits[kPlaceA][0].z = std::numeric_limits<float>::infinity();
    EXPECT_INVALID(non_finite);
    liminal::ObjectInstance extra;
    extra.id = "orphan-instance";
    extra.label = "orphan-instance";
    extra.portable = true;
    s.objects.instances[extra.id] = extra;
    EXPECT_INVALID(s);
}

void TestLegacyStateAndSerialization() {
    liminal::SessionState s;
    s.hard_state = liminal::MakeInitialHardState();
    s.hard_state.inventory_items.assign(1, "unknown relic");
    s.current_place_id = kPlaceA;
    s.objects.revision = 4;
    liminal::ObjectInstance relic;
    relic.id = "unknown relic"; relic.label = "unknown relic";
    relic.origin_place = kPlaceA; relic.portable = false;
    s.objects.instances[relic.id] = relic;
    liminal::EnsureObjectRegistry(&s);
    EXPECT_TRUE(s.objects.instances.size() == 1);
    EXPECT_TRUE(s.hard_state.inventory_items.size() == 1);
    if (s.hard_state.inventory_items.size() == 1)
        EXPECT_STRING(s.hard_state.inventory_items[0], "unknown relic");
    const auto it = s.objects.instances.find("unknown relic");
    EXPECT_TRUE(it != s.objects.instances.end());
    EXPECT_TRUE(it != s.objects.instances.end() && !it->second.portable);
    EXPECT_TRUE(s.objects.instances.count(kChalk) == 0);
    const auto p = DropPlacement("unknown relic");
    EXPECT_REFUSED(s, DropProposal("unknown relic", s.objects.revision), &p);
    liminal::SessionState legacy;legacy.hard_state.inventory_items={"unknown keepsake"};
    liminal::EnsureObjectRegistry(&legacy);
    EXPECT_TRUE(legacy.objects.instances.size()==1&&!legacy.objects.instances.at("unknown keepsake").portable);
    const std::string json = liminal::SerializeObjectState(s.objects);
    liminal::ObjectState parsed; std::string error;
    EXPECT_TRUE(liminal::ParseObjectState(json, &parsed, &error));
    EXPECT_STRING(liminal::SerializeObjectState(parsed), json);
    liminal::ObjectState ignored; std::string bad_error;
    EXPECT_FALSE(liminal::ParseObjectState("{ invalid json", &ignored, &bad_error));
    EXPECT_FALSE(bad_error.empty());
    liminal::ObjectState future = s.objects; future.version = 99;
    EXPECT_FALSE(liminal::ParseObjectState(liminal::SerializeObjectState(future), &ignored, &error));
}

void TestMatchLocalObjects() {
    liminal::SessionState s = MakeFixture();
    AddLocalObject(s, "dep-a", kPlaceA, 0.0f);
    AddLocalObject(s, "dep-b", kPlaceA, 1.0f);
    AddLocalObject(s, "far-caillou", kPlaceB, 0.0f);
    const auto local = liminal::MatchLocalObjects(s, "caillou");
    EXPECT_TRUE(local.size() == 2 && Contains(local, "dep-a") && Contains(local, "dep-b"));
    EXPECT_FALSE(Contains(local, "far-caillou"));
    const auto explicit_id = liminal::MatchLocalObjects(s, "dep-b");
    EXPECT_TRUE(explicit_id.size() == 1 && explicit_id[0] == "dep-b");
}

}  // namespace

void RunObjectConservationTests() {
    TestInitialRegistry(); TestExplicitDrop(); TestTravelAndReturn(); TestDoubleMovesAndStaleRevision();
    TestAllThreeObjects(); TestInvalidDropRequests();
    TestImplicitTakeAndDepositInB(); TestMaterialSourceAndInterdictions();
    TestInvalidPartitions(); TestLegacyStateAndSerialization(); TestMatchLocalObjects();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    if(g_failures)throw std::runtime_error("Object conservation checks failed");
}
