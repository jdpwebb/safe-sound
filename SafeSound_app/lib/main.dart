import 'dart:async';

import 'package:flutter/material.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:safe_sound_app/azure_iot_hub.dart';
import 'control_page.dart';
import 'events_page.dart';
import 'config_page.dart';

const String sharedAccessKey = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
const String deviceID =
    "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
const String iotHubEndpoint = "Daedalus.azure-devices.net";

void main() => runApp(AppMain());

class AppMain extends StatelessWidget {
  // This widget is the root of the application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Safe Sound',
      theme: ThemeData(
        primarySwatch: Colors.blueGrey,
        textTheme: TextTheme(
          body1: TextStyle(fontSize: 16.7, height: 1.7),
          subhead: TextStyle(fontSize: 18.5, fontWeight: FontWeight.w500),
          button: TextStyle(fontSize: 17.7),
          // title: TextStyle(fontSize: 36.0, fontStyle: FontStyle.italic),
          // body1: TextStyle(fontSize: 14.0, fontFamily: 'Hind'),
        ),
        buttonTheme: ButtonThemeData(
          height: 50,
          minWidth: 175,
          buttonColor: Colors.blueGrey[300],
        ),
      ),
      home: AppConfig(
        child: MainPage(),
      ),
    );
  }
}

class AppConfig extends StatefulWidget {
  AppConfig({
    Key key,
    this.child,
  }) : super(key: key);

  final Widget child;

  @override
  AppConfigState createState() => AppConfigState();

  static AppConfigState of(BuildContext context) {
    return (context.inheritFromWidgetOfExactType(_InheritedConfig)
            as _InheritedConfig)
        .data;
  }
}

class AppConfigState extends State<AppConfig> {
  DateTime _lastUpdateTime;

  DateTime get lastUpdateTime => _lastUpdateTime;

  void updateLastUpdateTime() {
    _lastUpdateTime = DateTime.now();
  }

  @override
  Widget build(BuildContext context) {
    return _InheritedConfig(
      data: this,
      child: widget.child,
    );
  }
}

class _InheritedConfig extends InheritedWidget {
  static _InheritedConfig of(BuildContext context) =>
      context.inheritFromWidgetOfExactType(_InheritedConfig);

  _InheritedConfig({Key key, Widget child, this.data})
      : super(key: key, child: child);

  final AppConfigState data;

  @override
  bool updateShouldNotify(_InheritedConfig oldWidget) {
    return oldWidget.data.lastUpdateTime != this.data.lastUpdateTime;
  }
}

typedef void UpdateTwinCB(Map<String, dynamic> update);

class MainPage extends StatefulWidget {
  MainPage({Key key}) : super(key: key);

  @override
  _MainPageState createState() => _MainPageState();
}

class _MainPageState extends State<MainPage> {
  static const pageNames = const ["Events", "Control", "Configure"];
  int _tabIndex;
  AzureIoTHub iotHub = AzureIoTHub(
    iotHubEndpoint: iotHubEndpoint,
    sharedAccessKey: sharedAccessKey,
  );
  static Future<Map<String, dynamic>> twinData;
  final FirebaseMessaging _firebaseMessaging = FirebaseMessaging();

  Widget _buildNotificationDialog(BuildContext context, String message) {
    return AlertDialog(
      title: Text("Security Event"),
      content: Text("$message"),
      actions: <Widget>[
        FlatButton(
          child: const Text('OK'),
          onPressed: () {
            Navigator.pop(context, false);
          },
        ),
      ],
    );
  }

  void _showNotification(Map<dynamic, dynamic> message) {
    showDialog<bool>(
      context: context,
      builder: (_) => _buildNotificationDialog(context, message['body']),
    );
  }

  @override
  void initState() {
    super.initState();
    _tabIndex = 1; // Show control page by default
    _firebaseMessaging.configure(
      onMessage: (Map<String, dynamic> message) async {
        print("onMessage: $message");
        this._showNotification(message['notification']);
      },
      onLaunch: (Map<String, dynamic> message) async {
        print("onLaunch: $message");
        _tabIndex = 0;
      },
      onResume: (Map<String, dynamic> message) async {
        print("onResume: $message");
        _tabIndex = 0;
      },
    );
    _firebaseMessaging.requestNotificationPermissions();
    _firebaseMessaging.subscribeToTopic('events');

    twinUpdate();
  }

  @override
  Widget build(BuildContext context) {
    Widget page;
    switch (_tabIndex) {
      case 0:
        page = EventsPage(
          deviceTwin: twinData,
          iotHub: iotHub,
        );
        break;
      case 1:
        page = ControlPage(
          deviceTwin: twinData,
          updateTwinCB: twinUpdate,
        );
        break;
      case 2:
        page = ConfigPage(
          deviceTwin: twinData,
          updateTwinCB: twinUpdate,
        );
        break;
    }
    return Scaffold(
      appBar: AppBar(
        title: Text(pageNames[_tabIndex]),
      ),
      body: Container(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.bottomLeft,
            end: Alignment.topRight,
            stops: [0.08, 0.9],
            colors: [
              Colors.indigoAccent[100].withOpacity(0.35),
              Colors.white,
            ],
          ),
        ),
        child: Center(
          child: page,
        ),
      ),
      bottomNavigationBar: BottomNavigationBar(
        iconSize: 30.0,
        unselectedFontSize: 13.5,
        selectedFontSize: 14.5,
        backgroundColor: Colors.blueGrey[50],
        currentIndex: _tabIndex,
        onTap: _onTabTapped,
        items: <BottomNavigationBarItem>[
          BottomNavigationBarItem(
            icon: Icon(Icons.alarm),
            title: Text(pageNames[0]),
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.security),
            title: Text(pageNames[1]),
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.settings),
            title: Text(pageNames[2]),
          ),
        ],
      ),
    );
  }

  void _onTabTapped(int index) {
    setState(() {
      _tabIndex = index;
    });
  }

  void twinUpdate([Map<String, dynamic> update]) {
    Future<Map<String, dynamic>> updateFuture;
    if (update == null) {
      updateFuture = iotHub.retrieveDeviceTwin(deviceID: deviceID);
    } else {
      updateFuture =
          iotHub.updateDeviceTwin(deviceID: deviceID, updates: update);
    }
    twinData = updateFuture.then((data) {
      print("Received twin update");
      // add the time that the future completed to the app state
      AppConfig.of(context).updateLastUpdateTime();
      setState(() {});
      return data;
    }).catchError((e) {
      Scaffold.of(context).showSnackBar(SnackBar(content: Text(e.toString())));
      print("error communicating with hub");
    });
  }
}
