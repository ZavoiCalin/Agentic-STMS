import React from 'react';
import { StatusBar } from 'expo-status-bar';
import RouteMapScreen from './src/screens/RouteMapScreen';

export default function App() {
  return (
    <>
      <StatusBar style="auto" />
      <RouteMapScreen />
    </>
  );
}
