import 'package:flutter/widgets.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import 'main.dart';

class ControlPage extends StatelessWidget {
  final UpdateTwinCB updateTwinCB;
  final Future<Map<String, dynamic>> deviceTwin;
  ControlPage({
    @required this.updateTwinCB,
    @required this.deviceTwin,
  });

  @override
  Widget build(BuildContext context) {
    return FutureBuilder<Map<String, dynamic>>(
      future: this.deviceTwin,
      builder:
          (BuildContext context, AsyncSnapshot<Map<String, dynamic>> snapshot) {
        Color disabledColor = Theme.of(context).disabledColor;
        bool isArmed = false;
        Widget connectionStatus;
        String connectionStatusTitle = "Security Device Connection Status";
        Widget armedStatus;
        String armedStatusTitle = "Security Device Status";
        Widget armedDescription = Text("");
        switch (snapshot.connectionState) {
          case ConnectionState.none:
          case ConnectionState.active:
          case ConnectionState.waiting:
            connectionStatus = ControlItem(
              title: connectionStatusTitle,
              icon: Icon(Icons.wifi, color: disabledColor),
            );
            armedStatus = ControlItem(
              title: armedStatusTitle,
              icon: Icon(Icons.lock, color: disabledColor),
            );
            break;
          case ConnectionState.done:
            if (snapshot.hasError) {
              connectionStatus = ControlItem(
                title: connectionStatusTitle,
                icon: Icon(Icons.wifi, color: disabledColor),
                status: Text('Unknown'),
              );
              armedStatus = ControlItem(
                title: armedStatusTitle,
                icon: Icon(Icons.lock, color: disabledColor),
                status: Text('Unknown'),
              );
              armedDescription =
                  Text("Error retrieving device info: ${snapshot.error}");
            } else {
              bool isDeviceConnected =
                  snapshot.data['connectionState'] == 'Connected';
              connectionStatus = ControlItem(
                title: connectionStatusTitle,
                icon: Icon(
                  Icons.wifi,
                  color: isDeviceConnected ? Colors.green : Colors.red,
                ),
                status: Text(snapshot.data['connectionState']),
              );
              isArmed = snapshot.data['properties']['reported']['armed'];
              armedStatus = ControlItem(
                title: armedStatusTitle,
                icon: Icon(
                  isArmed ? Icons.lock : Icons.lock_open,
                  color: isArmed ? Colors.green : Colors.red,
                ),
                status: Text(isArmed ? 'Armed' : 'Disarmed'),
              );
              armedDescription = Text(
                  "Arming the system will enable alarms and notifications.");
              if (isArmed) {
                armedDescription = FractionallySizedBox(
                  widthFactor: 0.7,
                  child: Text(
                    "Disarming the system will silence all alarms and notifications.",
                    textAlign: TextAlign.center,
                  ),
                );
              }
            }
        }
        DateTime lastUpdate = AppConfig.of(context).lastUpdateTime;
        var formatter = new DateFormat.jms();
        String lastUpdateStatus =
            lastUpdate == null ? "Unknown" : formatter.format(lastUpdate);
        Widget lastUpdateTime = ControlItem(
            title: "Last Status Update", status: Text(lastUpdateStatus));

        return Container(
          padding: EdgeInsets.all(30),
          child: Column(
            children: <Widget>[
              connectionStatus,
              SizedBox(height: 25),
              armedStatus,
              SizedBox(height: 25),
              lastUpdateTime,
              SizedBox(height: 0.12 * MediaQuery.of(context).size.height),
              armedDescription,
              SizedBox(height: 60),
              RaisedButton(
                child: Text(isArmed ? 'Disarm' : 'Arm'),
                onPressed: isArmed == null ? null : () => updateArm(!isArmed),
              ),
              SizedBox(height: 50),
            ],
          ),
        );
      },
    );
  }

  void updateArm(bool shouldArm) {
    var updates = {'armed': shouldArm};
    this.updateTwinCB(updates);
  }
}

class ControlItem extends StatelessWidget {
  final String title;
  final Icon icon;
  final Widget status;
  ControlItem({
    this.title = "",
    this.icon,
    this.status = const Text("Loading..."),
  });
  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: <Widget>[
        Text(title, style: Theme.of(context).textTheme.subhead),
        SizedBox(height: 5),
        Row(
          mainAxisSize: MainAxisSize.min,
          children: <Widget>[
            if (icon != null)
              Padding(
                padding: EdgeInsets.only(right: 12),
                child: icon,
              ),
            status,
          ],
        ),
      ],
    );
  }
}
