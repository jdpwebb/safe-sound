import 'package:flutter/widgets.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import 'azure_iot_hub.dart';
import 'main.dart';

class EventsPage extends StatefulWidget {
  final Future deviceTwin;
  final AzureIoTHub iotHub;
  const EventsPage({
    @required this.iotHub,
    this.deviceTwin,
  });

  @override
  EventsPageState createState() => EventsPageState();
}

class EventsPageState extends State<EventsPage> {
  @override
  Widget build(BuildContext context) {
    return FutureBuilder<Map<String, dynamic>>(
      future: widget.deviceTwin,
      builder:
          (BuildContext context, AsyncSnapshot<Map<String, dynamic>> snapshot) {
        bool isConnected = false;
        bool noEvents = true;
        Widget events;

        switch (snapshot.connectionState) {
          case ConnectionState.none:
          case ConnectionState.active:
          case ConnectionState.waiting:
            events = Text("Loading...");
            break;
          case ConnectionState.done:
            if (snapshot.hasError) {
              events = Text("Error retrieving data.");
            } else {
              isConnected = true;
              var eventsList =
                  snapshot.data['properties']['reported']['eventHistory'];
              var eventsWidgetList = <Widget>[];
              Map<String, dynamic> currentEvent;
              var currentIndex = 0;
              while (eventsList != null) {
                currentEvent = eventsList[currentIndex.toString()];
                ++currentIndex;
                if (currentEvent == null) {
                  break;
                }
                String eventString = 'Event ';
                switch (currentEvent['eventType']) {
                  case 'window_break':
                    eventString = 'Window breaking';
                    break;
                  case 'gunshot':
                    eventString = 'Gunshot';
                    break;
                }
                DateTime eventTime = DateTime.fromMillisecondsSinceEpoch(
                  currentEvent['eventTime'] * Duration.millisecondsPerSecond,
                  isUtc: true,
                );
                var formatter = DateFormat.jms().add_yMd();
                eventString += ' at ${formatter.format(eventTime.toLocal())}';
                var newEvent = Card(
                  color: Colors.blueGrey[100],
                  margin: EdgeInsets.symmetric(vertical: 7),
                  child: Center(
                    child: Text(
                      eventString,
                      style: TextStyle(fontSize: 17.5),
                    ),
                  ),
                );
                eventsWidgetList.add(newEvent);
              }
              noEvents = eventsWidgetList.length == 0;
              if (noEvents) {
                events = Text('No recent events');
              } else {
                events = ListView(
                  itemExtent: 63,
                  children: eventsWidgetList,
                );
              }
            }
        }
        return Column(
          children: <Widget>[
            Container(
              height: 200,
              margin: EdgeInsets.symmetric(
                horizontal: 42,
                vertical: 30,
              ),
              alignment: Alignment.center,
              child: events,
            ),
            RaisedButton(
              child: Text("Clear Events"),
              onPressed: noEvents ? null : clearEvents,
            ),
            SizedBox(height: 0.19 * MediaQuery.of(context).size.height),
            RaisedButton(
              child: Text("Simulate Event"),
              onPressed: !isConnected ? null : requestSimulation,
            ),
          ],
        );
      },
    );
  }

  void clearEvents() async {
    print("clear events");
    try {
      await widget.iotHub
          .callDirectMethod(deviceID: deviceID, methodName: "clearHistory");
      setState(() {});
    } catch (e) {
      Scaffold.of(context).showSnackBar(SnackBar(content: Text(e.toString())));
    }
  }

  void requestSimulation() async {
    try {
      await widget.iotHub
          .callDirectMethod(deviceID: deviceID, methodName: "simulateEvent");
    } catch (e) {
      Scaffold.of(context).showSnackBar(SnackBar(content: Text(e.toString())));
    }
  }
}
