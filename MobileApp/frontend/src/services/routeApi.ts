import { Platform } from 'react-native';
import { RouteResponse } from '../types/route';

const API_BASE_URL = (() => {
  if (process.env.EXPO_PUBLIC_API_URL) {
    return process.env.EXPO_PUBLIC_API_URL;
  }

  if (Platform.OS === 'android') {
    return 'http://10.0.2.2:3000';
  }

  if (Platform.OS === 'ios') {
    return 'http://127.0.0.1:3000';
  }

  return 'http://localhost:3000';
})();

export const API_URL = `${API_BASE_URL}/api/routes/latest`;

export async function fetchLatestRoute(): Promise<RouteResponse> {
  const response = await fetch(API_URL, {
    method: 'GET',
    headers: {
      Accept: 'application/json',
      'Content-Type': 'application/json',
    },
  });

  if (!response.ok) {
    throw new Error(`Failed to fetch route: ${response.status}`);
  }

  return response.json() as Promise<RouteResponse>;
}
