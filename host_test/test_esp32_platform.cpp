// Tests the real adapter against an injected IDF boundary, not physical flash.
#include <gtest/gtest.h>
#include <neondoll/esp32_platform.hpp>
#include <neondoll/identity_storage.hpp>
#include <cstring>
#include <map>

namespace {
struct FakeNvs {
    int init_error=0, open_error=0, read_error=0, set_error=0, commit_error=0;
    int commits=0, closes=0, opens=0, entropy_enable=0, entropy_disable=0;
    // Commit outcomes model atomic old/new and detectable damaged records.
    int commit_outcome=0; // 0: old; 1: new despite error; 2: partial despite error
    bool enabled=false, fail_identity_set=false;
    std::map<std::string,std::string> durable, staged;
} fake;
class AdapterTest : public testing::Test {
    void SetUp() override { fake=FakeNvs{}; }
};
}
esp_err_t nvs_flash_init(){return fake.init_error;}
esp_err_t nvs_open(const char* name,int mode,nvs_handle_t* handle){
    EXPECT_STREQ(name,"neondoll"); EXPECT_EQ(mode,NVS_READWRITE);
    ++fake.opens; *handle=1; return fake.open_error;
}
void nvs_close(nvs_handle_t){++fake.closes; fake.staged.clear();}
esp_err_t nvs_get_blob(nvs_handle_t,const char* key,void* out,size_t* size){
    if(fake.read_error)return fake.read_error;
    auto it=fake.durable.find(key);
    if(it==fake.durable.end())return ESP_ERR_NVS_NOT_FOUND;
    if(!out){*size=it->second.size();return ESP_OK;}
    if(*size<it->second.size())return ESP_ERR_NVS_INVALID_LENGTH;
    *size=it->second.size();std::memcpy(out,it->second.data(),*size);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t,const char* key,const void* data,size_t size){
    if(fake.set_error)return fake.set_error;
    if(fake.fail_identity_set && std::strcmp(key,"identity")==0)return ESP_FAIL;
    fake.staged[key]=std::string(static_cast<const char*>(data),size);return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t){
    ++fake.commits;
    if(!fake.commit_error || fake.commit_outcome==1) {
        for(const auto& item:fake.staged)fake.durable[item.first]=item.second;
    } else if(fake.commit_outcome==2) {
        for(const auto& item:fake.staged)fake.durable[item.first]=item.second.substr(0,10);
    }
    return fake.commit_error;
}
void bootloader_random_enable(){++fake.entropy_enable;fake.enabled=true;}
void bootloader_random_disable(){++fake.entropy_disable;fake.enabled=false;}
void esp_fill_random(void* out,size_t size){EXPECT_TRUE(fake.enabled);std::memset(out,0xab,size);}
int64_t esp_timer_get_time(){return 1234567;}
void esp_log_write(int,const char*,const char*,...){}

TEST_F(AdapterTest, CommittedRecordSurvivesNewAdapterAndMapsLongLogicalKey){
    {
        neondoll::Esp32Storage storage;
        ASSERT_EQ(storage.init(),0);
        ASSERT_EQ(storage.write("neondoll/identity","abc",3),0);
        EXPECT_EQ(fake.commits,2);
        EXPECT_EQ(fake.durable.at("identity"),"abc");
    }
    neondoll::Esp32Storage restarted;ASSERT_EQ(restarted.init(),0);
    char out[8];EXPECT_EQ(restarted.read("neondoll/identity",out,sizeof(out)),3);
    EXPECT_EQ(std::string(out,3),"abc");
    EXPECT_EQ(restarted.write("this-key-is-too-long","x",1),-3);
}
TEST_F(AdapterTest, MissingCorruptTooLargeAndIoAreDistinct){
    neondoll::Esp32Storage storage;ASSERT_EQ(storage.init(),0);
    char out[2];EXPECT_EQ(storage.read("identity",out,2),-1);
    fake.durable["identity"]="";EXPECT_EQ(storage.read("identity",out,2),0);
    fake.durable["identity"]="abc";EXPECT_EQ(storage.read("identity",out,2),-2);
    fake.read_error=ESP_ERR_NVS_TYPE_MISMATCH;EXPECT_EQ(storage.read("identity",out,2),-4);
    fake.read_error=ESP_FAIL;EXPECT_EQ(storage.read("identity",out,2),-3);
}
TEST_F(AdapterTest, InitFailureNeverOpensOrErasesExistingState){
    fake.durable["identity"]="existing";
    for(int err:{ESP_ERR_NVS_NO_FREE_PAGES,ESP_ERR_NVS_NEW_VERSION_FOUND,ESP_FAIL}){
        fake.init_error=err;
        neondoll::Esp32Storage storage;EXPECT_NE(storage.init(),0);
        EXPECT_EQ(fake.durable.at("identity"),"existing");
    }
    EXPECT_EQ(fake.opens,0);
}
TEST_F(AdapterTest, SetAndCommitFailuresAreReportedAndPoisonHandle){
    neondoll::Esp32Storage storage;ASSERT_EQ(storage.init(),0);
    fake.set_error=ESP_FAIL;
    EXPECT_NE(storage.write("identity","x",1),0);EXPECT_EQ(fake.commits,0);
    char out[8];EXPECT_EQ(storage.read("identity",out,8),-3);
    storage.deinit();fake.set_error=0;ASSERT_EQ(storage.init(),0);
    fake.commit_error=ESP_FAIL;
    EXPECT_NE(storage.write("identity","x",1),0);EXPECT_EQ(fake.commits,1);
    EXPECT_NE(storage.write("identity","x",1),0);
}
TEST_F(AdapterTest, InterruptedLegacyUpgradeKeepsOldOrNewIdAndRejectsPartial){
    for(int outcome:{0,1,2}){
        fake=FakeNvs{};
        fake.durable["identity"]="v1:0123456789abcdef0123456789abcdef:0:0";
        fake.durable["identity_init"]=std::string("NDI1",5);
        neondoll::Esp32Storage storage;ASSERT_EQ(storage.init(),0);
        neondoll::IdentityState state{};
        ASSERT_EQ(neondoll::load_identity(&storage,"neondoll/identity",state),0);
        state.version=neondoll::CURRENT_IDENTITY_VERSION;
        fake.commit_error=ESP_FAIL;fake.commit_outcome=outcome;
        EXPECT_NE(neondoll::save_identity(&storage,"neondoll/identity",state),0);
        storage.deinit();fake.commit_error=0;ASSERT_EQ(storage.init(),0);
        neondoll::IdentityState loaded{};
        int ret=neondoll::load_identity(&storage,"neondoll/identity",loaded);
        if(outcome==2)EXPECT_EQ(ret,-4);
        else {ASSERT_EQ(ret,0);EXPECT_EQ(loaded.identity.body_id,state.identity.body_id);}
    }
}
TEST_F(AdapterTest, BootstrapEntropyRequiresInitAndBalancesHardwareSource){
    neondoll::Esp32EntropySource entropy;unsigned char bytes[16]{};
    EXPECT_NE(entropy.get_random(bytes,sizeof(bytes)),0);
    ASSERT_EQ(entropy.init(),0);
    EXPECT_NE(entropy.get_random(nullptr,16),0);
    ASSERT_EQ(entropy.get_random(bytes,sizeof(bytes)),0);
    for(auto byte:bytes)EXPECT_EQ(byte,0xab);
    EXPECT_EQ(fake.entropy_enable,1);EXPECT_EQ(fake.entropy_disable,1);EXPECT_FALSE(fake.enabled);
    entropy.deinit();EXPECT_NE(entropy.get_random(bytes,sizeof(bytes)),0);
    neondoll::Esp32Clock clock;EXPECT_EQ(clock.now_ms(),1234u);
}

TEST_F(AdapterTest, MarkerWithoutIdentityRefusesFirstBootReplacement) {
    fake.durable["identity_init"]=std::string("NDI1",5);
    neondoll::Esp32Storage storage; ASSERT_EQ(storage.init(),0);
    neondoll::IdentityState state{};
    EXPECT_EQ(neondoll::load_identity(&storage,"neondoll/identity",state),-4);
    EXPECT_EQ(fake.commits,0);
}
TEST_F(AdapterTest, CorruptMarkerFailsClosedEvenWithReadableIdentity) {
    fake.durable["identity_init"]="broken";
    fake.durable["identity"]="v1:0123456789abcdef0123456789abcdef:0:0";
    neondoll::Esp32Storage storage; ASSERT_EQ(storage.init(),0);
    neondoll::IdentityState state{};
    EXPECT_EQ(neondoll::load_identity(&storage,"neondoll/identity",state),-4);
    EXPECT_NE(storage.write("identity_init","x",1),0);
}

TEST_F(AdapterTest, FirstBootPowerCutAfterMarkerNeverLooksLikeCleanFlash) {
    fake.fail_identity_set=true;
    neondoll::Esp32Storage storage; ASSERT_EQ(storage.init(),0);
    EXPECT_NE(storage.write("neondoll/identity","new identity",12),0);
    EXPECT_EQ(fake.commits,1);
    storage.deinit(); fake.fail_identity_set=false;
    ASSERT_EQ(storage.init(),0);
    char out[32]; EXPECT_EQ(storage.read("neondoll/identity",out,sizeof(out)),-4);
    EXPECT_EQ(fake.durable.count("identity"),0u);
}
TEST_F(AdapterTest, OpenFailureAndRepeatedInitHaveCorrectHandleOwnership) {
    fake.open_error=ESP_FAIL;
    neondoll::Esp32Storage storage; EXPECT_NE(storage.init(),0);
    EXPECT_EQ(storage.deinit(),0); EXPECT_EQ(fake.closes,0);
    fake.open_error=0; ASSERT_EQ(storage.init(),0);
    const int opens=fake.opens;
    EXPECT_EQ(storage.init(),0); EXPECT_EQ(fake.opens,opens);
    storage.deinit(); storage.deinit(); EXPECT_EQ(fake.closes,1);
}
