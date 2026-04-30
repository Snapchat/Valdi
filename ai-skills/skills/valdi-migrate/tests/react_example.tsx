// React example — hooks, Context, FlatList, fetch, styled-components.
// Goal: migrate this to Valdi.

import React, { useState, useEffect, useContext, createContext } from 'react';
import { View, Text, FlatList, TouchableOpacity, StyleSheet } from 'react-native';
import styled from 'styled-components/native';

// --- Context / theme ---
const ThemeContext = createContext({ primary: '#FFFC00' });

// --- Stateless greeting ---
const Greeting: React.FC<{ name: string }> = ({ name }) => {
  return <Text>Hello, {name}</Text>;
};

// --- Stateful counter ---
const Counter: React.FC<{ label: string }> = ({ label }) => {
  const [count, setCount] = useState(0);
  return (
    <TouchableOpacity onPress={() => setCount(c => c + 1)}>
      <Text>{label}: {count}</Text>
    </TouchableOpacity>
  );
};

// --- Data fetching with useEffect ---
interface User { id: string; name: string; }

const UserList: React.FC = () => {
  const [users, setUsers] = useState<User[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    let cancelled = false;
    fetch('https://api.example.com/users')
      .then(r => r.json())
      .then(data => {
        if (!cancelled) {
          setUsers(data);
          setLoading(false);
        }
      });
    return () => { cancelled = true; };
  }, []);

  if (loading) return <Text>Loading...</Text>;

  return (
    <FlatList
      data={users}
      keyExtractor={u => u.id}
      renderItem={({ item }) => <Text>{item.name}</Text>}
    />
  );
};

// --- styled-components ---
const Card = styled.View`
  background-color: #ffffff;
  border-radius: 12px;
  padding: 16px;
  shadow-color: #000;
  shadow-opacity: 0.15;
  shadow-radius: 8px;
`;

const CardTitle = styled.Text`
  font-size: 18px;
  font-weight: bold;
`;

const ProfileCard: React.FC<{ name: string; bio: string }> = ({ name, bio }) => (
  <Card>
    <CardTitle>{name}</CardTitle>
    <Text>{bio}</Text>
  </Card>
);

// --- Context consumer ---
const ThemedBadge: React.FC<{ text: string }> = ({ text }) => {
  const theme = useContext(ThemeContext);
  return (
    <View style={{ backgroundColor: theme.primary }}>
      <Text>{text}</Text>
    </View>
  );
};

// --- Re-render bug: inline lambda + map() ---
const BadList: React.FC<{ items: { id: string; label: string }[] }> = ({ items }) => (
  <View>
    {items.map(item => (
      <TouchableOpacity key={item.id} onPress={() => console.log(item.id)}>
        <Text>{item.label}</Text>
      </TouchableOpacity>
    ))}
  </View>
);
