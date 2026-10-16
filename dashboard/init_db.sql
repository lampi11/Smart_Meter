-- Enable UUID generation
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- organizations (landlords/PG operators)
CREATE TABLE organizations (
  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),
  name text NOT NULL,
  phone text,
  email text,
  created_at timestamptz DEFAULT now()
);

-- properties (buildings/PGs belonging to an organization)
CREATE TABLE properties (
  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),
  organization_id uuid REFERENCES organizations(id),
  name text NOT NULL,
  address text,
  created_at timestamptz DEFAULT now()
);

-- meters (the physical ESP32 devices)
CREATE TABLE meters (
  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),
  device_id text UNIQUE NOT NULL, -- The MAC address from the ESP32
  organization_id uuid REFERENCES organizations(id),
  property_id uuid REFERENCES properties(id),
  qr_code text UNIQUE, -- e.g. lampitex-qr-1234
  label text, -- e.g. "Room 101"
  balance numeric(10,2) NOT NULL DEFAULT 0,
  tariff_grid numeric(6,2) NOT NULL DEFAULT 8.0,
  emergency_credit_limit numeric(6,2) DEFAULT 30,
  relay_on boolean DEFAULT true,
  last_seen_at timestamptz,
  created_at timestamptz DEFAULT now()
);

-- readings (telemetry from ESP32)
CREATE TABLE readings (
  id bigserial PRIMARY KEY,
  meter_id uuid REFERENCES meters(id),
  recorded_at timestamptz NOT NULL,
  voltage numeric(6,2),
  current numeric(8,3),
  power numeric(10,2),
  energy_kwh numeric(12,4),
  balance_after numeric(10,2),
  relay_on boolean
);
CREATE INDEX ON readings (meter_id, recorded_at DESC);

-- Seed Dummy Data
INSERT INTO organizations (id, name, email) VALUES 
  ('11111111-1111-1111-1111-111111111111', 'Lampitex Demo Org', 'demo@lampitex.com');

INSERT INTO properties (id, organization_id, name, address) VALUES
  ('22222222-2222-2222-2222-222222222222', '11111111-1111-1111-1111-111111111111', 'Demo PG', '123 Main St');

-- The meter we are currently testing (Assuming the MAC address is 8C:94:DF:4C:C1:04 or similar, we'll use a dummy MAC)
INSERT INTO meters (id, device_id, organization_id, property_id, qr_code, label, balance, tariff_grid) VALUES
  ('33333333-3333-3333-3333-333333333333', '8C:94:DF:90:3C:C4', '11111111-1111-1111-1111-111111111111', '22222222-2222-2222-2222-222222222222', 'LMPTX-101', 'Room 101', 95.50, 8.0);

-- Dummy readings
INSERT INTO readings (meter_id, recorded_at, voltage, current, power, energy_kwh, balance_after, relay_on) VALUES
  ('33333333-3333-3333-3333-333333333333', now() - interval '15 minutes', 230.1, 0.5, 115.0, 10.01, 95.60, true),
  ('33333333-3333-3333-3333-333333333333', now() - interval '10 minutes', 229.8, 0.5, 114.9, 10.02, 95.55, true),
  ('33333333-3333-3333-3333-333333333333', now() - interval '5 minutes',  230.5, 0.6, 138.3, 10.03, 95.50, true);
