#include "RoguelikeSettlementTaskPlugin.h"

#include "Config/TaskData.h"
#include "Controller/Controller.h"
#include "Vision/Matcher.h"
#include "Vision/RegionOCRer.h"

bool asst::RoguelikeSettlementTaskPlugin::verify(AsstMsg msg, const json::value& details) const
{
    if (msg != AsstMsg::SubTaskStart || details.get("subtask", std::string()) != "ProcessTask") {
        return false;
    }

    auto task_name = details.at("details").at("task").as_string();
    if (/*m_config->get_mode() == RoguelikeMode::Exp && */ task_name.ends_with("Roguelike@GamePass")) {
        m_game_pass = true;
        return true;
    }
    else if (/*m_config->get_mode() == RoguelikeMode::Exp && */ task_name.ends_with("Roguelike@MissionFailedFlag2")) {
        m_game_pass = false;
        return true;
    }
    else {
        return false;
    }
}

bool asst::RoguelikeSettlementTaskPlugin::_run()
{
    const static auto task = Task.get("RoguelikeSettlementConfirm");
    auto json_msg = basic_info_with_what("RoguelikeSettlement");
    json_msg["details"]["game_pass"] = m_game_pass;

    sleep(task->special_params[0]);
    save_img(utils::path("debug") / utils::path("roguelike") / utils::path("settlement"));

    const static auto rect = Task.get("Roguelike@ClickToStartPoint")->specific_rect;
    ctrler()->click(rect);
    sleep(task->pre_delay);

    cv::Mat image;
    if (!wait_for_whole_page(image)) {
        Log.error(__FUNCTION__, "wait for whole page failed");
        return true;
    }
    save_img(utils::path("debug") / utils::path("roguelike") / utils::path("settlement"));

    get_settlement_info(json_msg, image);
    callback(AsstMsg::SubTaskExtraInfo, json_msg);
    return true;
}

bool asst::RoguelikeSettlementTaskPlugin::get_settlement_info(json::value& info, const cv::Mat& image)
{
    auto append_data = [&](const std::string& task_name, const std::string& ocr_result) {
        int num = -1;
        if (!utils::chars_to_number(ocr_result, num)) {
            Log.error(__FUNCTION__, "convert str to int failed, task:", task_name, ", string:", ocr_result);
            return;
        }

        auto tag = task_name.substr(task_name.find("-") + 1);
        utils::tolowers(tag);
        info["details"][tag] = num;
    };

    auto analyze_battle_data = [&](const std::string& task_name) {
        RegionOCRer ocr(image);
        ocr.set_task_info(task_name);
        ocr.set_bin_threshold(0, 255);
        if (!ocr.analyze()) {
            Log.error(__FUNCTION__, "analyze battle data failed, task:", task_name);
            return;
        }
        append_data(task_name, ocr.get_result().text);
    };

    auto analyze_text_data = [&](const std::string& task_name) {
        RegionOCRer ocr(image);
        ocr.set_task_info(task_name);
        if (!ocr.analyze()) {
            Log.error(__FUNCTION__, "analyze battle data failed, task:", task_name);
            return;
        }
        auto tag = task_name.substr(task_name.find("-") + 1);
        utils::tolowers(tag);
        info["details"][tag] = ocr.get_result().text;
    };

    static const auto battle_task_name =
        std::vector<std::string> { "RoguelikeSettlementOcr-Floor",    "RoguelikeSettlementOcr-Step",
                                   "RoguelikeSettlementOcr-Combat",   "RoguelikeSettlementOcr-Recruit",
                                   "RoguelikeSettlementOcr-Object",   "RoguelikeSettlementOcr-BOSS",
                                   "RoguelikeSettlementOcr-Emergency" };
    static const auto text_task_name =
        std::vector<std::string> { "RoguelikeSettlementOcr-Difficulty", "RoguelikeSettlementOcr-Score",
                                   "RoguelikeSettlementOcr-Exp", "RoguelikeSettlementOcr-Skill" };

    ranges::for_each(battle_task_name, analyze_battle_data);
    ranges::for_each(text_task_name, analyze_text_data);

    return true;
}

bool asst::RoguelikeSettlementTaskPlugin::wait_for_whole_page(cv::Mat& image)
{
    Matcher matcher;
    matcher.set_task_info("RoguelikeSettlementConfirm");
    do {
        image = ctrler()->get_image();
        matcher.set_image(image);
        if (matcher.analyze()) {
            return true;
        }
        sleep(Config.get_options().task_delay);
    } while (!need_exit());
    return false;
}
