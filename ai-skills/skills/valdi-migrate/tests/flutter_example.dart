// Flutter example — mix of StatelessWidget, StatefulWidget, ListView.builder,
// navigation, SharedPreferences, and a Provider consumer.
// Goal: migrate this to Valdi.

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

// --- Theme service (ChangeNotifier) ---
class ThemeService extends ChangeNotifier {
  Color primary = Colors.yellow;
}

// --- Stateless greeting ---
class Greeting extends StatelessWidget {
  final String name;
  const Greeting({required this.name});

  @override
  Widget build(BuildContext context) {
    return Text('Hello, $name');
  }
}

// --- Stateful counter ---
class Counter extends StatefulWidget {
  final String label;
  const Counter({required this.label});
  @override
  State<Counter> createState() => _CounterState();
}

class _CounterState extends State<Counter> {
  int _count = 0;

  void _increment() {
    setState(() { _count++; });
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: _increment,
      child: Text('${widget.label}: $_count'),
    );
  }
}

// --- User list with async fetch and cancellation ---
class UserList extends StatefulWidget {
  @override
  State<UserList> createState() => _UserListState();
}

class _UserListState extends State<UserList> {
  List<String> _users = [];

  @override
  void initState() {
    super.initState();
    _loadUsers();
  }

  Future<void> _loadUsers() async {
    // simulated network fetch
    await Future.delayed(Duration(milliseconds: 500));
    setState(() { _users = ['Alice', 'Bob', 'Carol']; });
  }

  @override
  Widget build(BuildContext context) {
    return ListView.builder(
      itemCount: _users.length,
      itemBuilder: (context, index) {
        return ListTile(title: Text(_users[index]));
      },
    );
  }
}

// --- Settings screen using SharedPreferences ---
class SettingsScreen extends StatefulWidget {
  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  bool _notifications = false;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() { _notifications = prefs.getBool('notifications') ?? false; });
  }

  Future<void> _toggle() async {
    final prefs = await SharedPreferences.getInstance();
    final next = !_notifications;
    await prefs.setBool('notifications', next);
    setState(() { _notifications = next; });
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text('Notifications: $_notifications'),
        ElevatedButton(onPressed: _toggle, child: Text('Toggle')),
      ],
    );
  }
}

// --- Provider consumer ---
class ThemedButton extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    final theme = context.watch<ThemeService>();
    return Container(
      color: theme.primary,
      child: Text('Themed'),
    );
  }
}
