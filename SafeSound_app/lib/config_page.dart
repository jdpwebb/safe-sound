import 'dart:async';
import 'package:flutter/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'main.dart';

class ConfigPage extends StatefulWidget {
  final UpdateTwinCB updateTwinCB;
  final Future deviceTwin;
  ConfigPage({
    @required this.updateTwinCB,
    @required this.deviceTwin,
  });

  @override
  State<ConfigPage> createState() => ConfigPageState();
}

class ConfigPageState extends State<ConfigPage> {
  static const defaultDebounceTime = Duration(seconds: 5);
  Timer _debounceTimer;
  Map<String, dynamic> _propertyUpdates = <String, dynamic>{};

  @override
  Widget build(BuildContext context) {
    return Container(
      child: FutureBuilder<Map<String, dynamic>>(
        future: widget.deviceTwin,
        builder: (BuildContext context,
            AsyncSnapshot<Map<String, dynamic>> snapshot) {
          ConfigItem cooldownTime;
          String cooldownTimeLabel = "Event cooldown period (sec)";

          switch (snapshot.connectionState) {
            case ConnectionState.none:
            case ConnectionState.active:
            case ConnectionState.waiting:
              cooldownTime = ConfigItem(
                  label: cooldownTimeLabel, setting: Text("Loading..."));
              break;
            case ConnectionState.done:
              if (snapshot.hasError) {
                ConfigItem(
                    label: cooldownTimeLabel,
                    setting: Text("Error retrieving data."));
              } else {
                num cooldownSecs =
                    snapshot.data['properties']['desired']['eventCooldown'];
                if (cooldownSecs == null) {
                  cooldownSecs = 5;
                }
                cooldownTime = ConfigItem(
                  label: cooldownTimeLabel,
                  setting: ConstrainedBox(
                    constraints: BoxConstraints(maxWidth: 45),
                    child: TextField(
                      controller: TextEditingController(
                        text: cooldownSecs.toStringAsFixed(0),
                      ),
                      textAlignVertical: TextAlignVertical.center,
                      decoration: InputDecoration(
                        border: UnderlineInputBorder(),
                      ),
                      inputFormatters: [
                        WhitelistingTextInputFormatter.digitsOnly
                      ],
                      keyboardType: TextInputType.number,
                      onSubmitted: updateCooldownTime,
                    ),
                  ),
                );
              }
          }
          return Container(
            padding: EdgeInsets.symmetric(vertical: 30, horizontal: 40),
            child: Column(
              children: <Widget>[
                cooldownTime,
              ],
            ),
          );
        },
      ),
    );
  }

  void updateCooldownTime(String newNum) {
    _propertyUpdates['eventCooldown'] = int.tryParse(newNum);
    _debounceTimer?.cancel();
    _debounceTimer = Timer(defaultDebounceTime, () {
      sendUpdates();
    });
  }

  void sendUpdates() {
    widget.updateTwinCB(_propertyUpdates);
  }

  @override
  void dispose() {
    if (_debounceTimer != null && _debounceTimer.isActive) {
      _debounceTimer.cancel();
      sendUpdates();
    }
    super.dispose();
  }
}

class ConfigItem extends StatelessWidget {
  final String label;
  final Widget setting;
  ConfigItem({
    this.label,
    this.setting,
  });
  @override
  Widget build(BuildContext context) {
    return Center(
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: <Widget>[
          Padding(
            padding: EdgeInsets.only(right: 18),
            child: Text(
              label,
              style: TextStyle(
                fontSize: 17.5,
                fontWeight: FontWeight.w500,
              ),
            ),
          ),
          ConstrainedBox(
            constraints: BoxConstraints(maxHeight: 35),
            child: setting,
          ),
        ],
      ),
    );
  }
}
