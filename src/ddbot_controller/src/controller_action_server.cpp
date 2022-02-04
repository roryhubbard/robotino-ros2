#include <functional>
#include <memory>
#include <thread>

#include "ddbot_msgs/action/follow_path.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "rclcpp_components/register_node_macro.hpp"

namespace ddbot_controller
{
class ControllerActionServer : public rclcpp::Node
{
public:
  using FollowPath = ddbot_msgs::action::FollowPath;
  using GoalHandleFollowPath = rclcpp_action::ServerGoalHandle<FollowPath>;

  explicit ControllerActionServer(const rclcpp::NodeOptions & options = rclcpp::NodeOptions())
  : Node("controller_action_server", options)
  {
    using namespace std::placeholders;

    this->action_server_ = rclcpp_action::create_server<FollowPath>(
      this,
      "follow_path",
      std::bind(&ControllerActionServer::handle_goal, this, _1, _2),
      std::bind(&ControllerActionServer::handle_cancel, this, _1),
      std::bind(&ControllerActionServer::handle_accepted, this, _1));
  }

private:
  rclcpp_action::Server<FollowPath>::SharedPtr action_server_;

  rclcpp_action::GoalResponse handle_goal(
    const rclcpp_action::GoalUUID & uuid,
    std::shared_ptr<const FollowPath::Goal> goal)
  {
    RCLCPP_INFO(this->get_logger(),
                "Received follow path request with path size %lu", goal->poses.size());
    (void)uuid;
    return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
  }

  rclcpp_action::CancelResponse handle_cancel(
    const std::shared_ptr<GoalHandleFollowPath> goal_handle)
  {
    RCLCPP_INFO(this->get_logger(), "Received request to cancel goal");
    (void)goal_handle;
    return rclcpp_action::CancelResponse::ACCEPT;
  }

  void handle_accepted(const std::shared_ptr<GoalHandleFollowPath> goal_handle)
  {
    using namespace std::placeholders;
    // this needs to return quickly to avoid blocking the executor, so spin up a new thread
    std::thread{std::bind(&ControllerActionServer::execute, this, _1), goal_handle}.detach();
  }

  void execute(const std::shared_ptr<GoalHandleFollowPath> goal_handle)
  {
    RCLCPP_INFO(this->get_logger(), "Executing goal");
    rclcpp::Rate loop_rate(1);
    const auto goal = goal_handle->get_goal();
    auto feedback = std::make_shared<FollowPath::Feedback>();
    auto & number_of_poses_remaining = feedback->number_of_poses_remaining;
    number_of_poses_remaining = goal->poses.size();
    auto result = std::make_shared<FollowPath::Result>();

    for (unsigned long int i = 1; (i < goal->poses.size()) && rclcpp::ok(); ++i) {
      // Check if there is a cancel request
      if (goal_handle->is_canceling()) {
        result->success = false;
        goal_handle->canceled(result);
        RCLCPP_INFO(this->get_logger(), "Goal canceled");
        return;
      }
      // Update feedback
      number_of_poses_remaining = goal->poses.size() - i;
      // Publish feedback
      goal_handle->publish_feedback(feedback);
      RCLCPP_INFO(this->get_logger(), "Publish feedback");

      loop_rate.sleep();
    }

    // Check if goal is done
    if (rclcpp::ok()) {
      result->success = true;
      goal_handle->succeed(result);
      RCLCPP_INFO(this->get_logger(), "Goal succeeded");
    }
  }
};  // class ControllerActionServer

}  // namespace ddbot_controller

RCLCPP_COMPONENTS_REGISTER_NODE(ddbot_controller::ControllerActionServer)