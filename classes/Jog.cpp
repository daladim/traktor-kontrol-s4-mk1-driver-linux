#include "Jog.h"

map<int, Jog *> Jog::jog_mapping = {
        {52, new Jog(52, "JOG WHEEL CH1 / CH3", 0)},
        {53, new Jog(53, "JOG WHEEL CH2 / CH4", 0)},
        {26, new Jog(26, "JOG WHEEL CH1 / CH3 PRESSED", 0)},
        {27, new Jog(27, "JOG WHEEL CH1 / CH3 PRESSED", 0)}
};

Jog::Jog(){
  code = value = counter = -1;
  updated = {};
  sensitivity = 5;
  prev_control_value = -1000;
  name = "";
}

Jog::Jog(int in_code, string in_name, int in_value){
  code = in_code;
  name = in_name;
  value = in_value;
  sensitivity = 5;
  counter = 0;
  updated = {};
  prev_control_value = -1000;
}

int Jog::handle_event(RtMidiOut *midi_out, bool shift_ch1, bool shift_ch2, bool toggle_ac, bool toggle_bd, ConfigHelper *config_helper){
  shared_ptr<spdlog::logger> logger = spdlog::get(config_helper->get_string_value("traktor_s4_logger_name"));
  const int configured_sensitivity = config_helper->get_int_value("jog_wheel_sensitivity");
  if (configured_sensitivity > 0) {
    sensitivity = configured_sensitivity;
  }
  if (MidiEventOut::midi_mapping.find(code) != MidiEventOut::midi_mapping.end()) {
    MidiEventOut *midi_event = MidiEventOut::midi_mapping[code];
    logger->debug("[Jog::handle_event] Jog Wheel {0} with Code: {1} and Value {2}", name, code, value);
    logger->debug("[Jog::handle_event] Sending to MIDI with: Name: {0} Controller Type: {1} Status: {2} Channel: {3}", midi_event->name, midi_event->controller_type, midi_event->status_byte, midi_event->channel_byte);
    logger->debug("[Jog::handle_event] Creating message...");
    vector<int> midi_values;

    if (midi_event->controller_type == "JOG_TOUCH"){
      const int configured_threshold =
          config_helper->get_int_value("jog_touch_threshold");
      const int touch_threshold =
          configured_threshold > 0 ? configured_threshold : 3500;
      midi_values.push_back(value >= touch_threshold ? 0x7f : 0x00);
    }
    else{
      midi_values = get_values_jog();
    }

    for (int midi_value : midi_values) {
      auto message = UtilsHelper::create_message(shift_ch1, shift_ch2, toggle_ac,
                                                 toggle_bd, midi_event,
                                                 (unsigned char)midi_value);

      logger->debug("[Jog::handle_event] Message Channel: {0} Status: {1} Value: {2}", message[0], message[1], message[2]);
      logger->debug("[Jog::handle_event] Message created!");
      logger->debug("[Jog::handle_event] Sending to MIDI out port....");

      try{
        midi_out->sendMessage(&message);
      }
      catch (exception &e){
        logger->error("[Jog::handle_event] Error sending message to MIDI out port: {0}", e.what());
        return -1;
      }
      logger->debug("[Jog::handle_event] Sent!");
    }
  }

  return 0;
}

vector<int> Jog::get_values_jog(){
  if (prev_control_value == -1000){
    prev_control_value = value;
    updated = MidiEventOut::get_time();
    return {};
  }

  int diff = 0;
  if (value <= 255 && prev_control_value >= 767){
    diff = 1024 - prev_control_value + value;
  }
  else if (value >= 767 && prev_control_value <= 255){
    diff = value - 1024 - prev_control_value;
  }
  else{
    diff = value - prev_control_value;
  }

  prev_control_value = value;
  const auto now = MidiEventOut::get_time();
  if ((now - updated) < chrono::milliseconds(sensitivity)){
    counter += diff;
    return {};
  }

  int midi_value = counter + diff;
  counter = 0;
  updated = now;

  vector<int> midi_values;
  while (midi_value > 63) {
    midi_values.push_back(63);
    midi_value -= 63;
  }
  while (midi_value < -64) {
    midi_values.push_back(64);
    midi_value += 64;
  }
  if (midi_value > 0) {
    midi_values.push_back(midi_value);
  }
  else if (midi_value < 0) {
    midi_values.push_back(128 + midi_value);
  }

  return midi_values;
}
