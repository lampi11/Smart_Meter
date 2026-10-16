-- Enable the pg_cron extension (required for the daily cleanup job)
CREATE EXTENSION IF NOT EXISTS pg_cron;

-- ==========================================
-- TABLES
-- ==========================================

-- owners: landlords / property operators
CREATE TABLE owners (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  name text,
  phone text,
  email text,
  created_at timestamptz DEFAULT now()
);

-- properties: a building or PG
CREATE TABLE properties (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  owner_id uuid REFERENCES owners(id),
  name text,
  address text,
  created_at timestamptz DEFAULT now()
);

-- meters: one row per physical device
CREATE TABLE meters (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id text UNIQUE,              
  property_id uuid REFERENCES properties(id),
  label text,                         
  balance numeric(10,2) NOT NULL DEFAULT 0,
  tariff_grid numeric(6,2) NOT NULL,  
  tariff_dg numeric(6,2),             
  emergency_credit_limit numeric(6,2) DEFAULT 30,
  no_cut_start time DEFAULT '22:00',
  no_cut_end   time DEFAULT '07:00',
  relay_on boolean DEFAULT true,
  firmware_version text,
  last_seen_at timestamptz,           
  created_at timestamptz DEFAULT now()
);

-- tenants: occupants
CREATE TABLE tenants (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  name text,
  phone text,
  created_at timestamptz DEFAULT now()
);

-- assignment: who is on which meter, and under what arrangement
CREATE TABLE meter_assignments (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  meter_id uuid REFERENCES meters(id),
  tenant_id uuid REFERENCES tenants(id),
  assignment_type text CHECK (assignment_type IN ('owned','rented')),
  started_at timestamptz NOT NULL,
  ended_at timestamptz,               
  UNIQUE (meter_id, tenant_id, started_at)
);

-- readings: high-volume telemetry, 14-day rolling retention
CREATE TABLE readings (
  id bigserial PRIMARY KEY,
  meter_id uuid REFERENCES meters(id),
  recorded_at timestamptz NOT NULL,
  time_source text CHECK (time_source IN ('NTP','RTC','ESTIMATED','NONE')),
  voltage numeric(6,2),
  current numeric(8,3),
  power numeric(10,2),
  power_factor numeric(4,3),
  frequency numeric(5,2),
  energy_kwh numeric(12,4),
  balance_after numeric(10,2),
  source_mode text DEFAULT 'GRID',    
  relay_on boolean,
  synced_late boolean DEFAULT false    
);
-- Fast lookup index for querying the latest readings
CREATE INDEX ON readings (meter_id, recorded_at DESC);

-- ledger: append-only money trail, NEVER auto-deleted
CREATE TABLE ledger (
  id bigserial PRIMARY KEY,
  meter_id uuid REFERENCES meters(id),
  occurred_at timestamptz NOT NULL,
  entry_type text CHECK (entry_type IN ('recharge','consumption','adjustment','emergency_credit','repayment')),
  amount numeric(10,2) NOT NULL,       
  balance_after numeric(10,2) NOT NULL,
  payment_ref text,                    
  note text
);

-- alerts: raised by backend or device
CREATE TABLE alerts (
  id bigserial PRIMARY KEY,
  meter_id uuid REFERENCES meters(id),
  alert_type text,                     
  severity text,
  raised_at timestamptz DEFAULT now(),
  acknowledged_at timestamptz,
  detail jsonb
);

-- diagnostics: anonymous device health, 14-day retention
CREATE TABLE device_diagnostics (
  id bigserial PRIMARY KEY,
  device_id text NOT NULL,             
  reported_at timestamptz DEFAULT now(),
  firmware_version text,
  reset_reason text,
  free_heap integer,
  uptime_seconds bigint,
  wifi_rssi integer,
  error_counters jsonb
);

-- ==========================================
-- ROW LEVEL SECURITY (RLS) POLICIES
-- ==========================================

-- Enable RLS on tables that contain sensitive/tenant data
ALTER TABLE owners ENABLE ROW LEVEL SECURITY;
ALTER TABLE properties ENABLE ROW LEVEL SECURITY;
ALTER TABLE meters ENABLE ROW LEVEL SECURITY;
ALTER TABLE tenants ENABLE ROW LEVEL SECURITY;
ALTER TABLE meter_assignments ENABLE ROW LEVEL SECURITY;
ALTER TABLE readings ENABLE ROW LEVEL SECURITY;
ALTER TABLE ledger ENABLE ROW LEVEL SECURITY;

-- 1. Owners see only their own properties and meters
CREATE POLICY "Owners can view their own properties" 
  ON properties FOR SELECT 
  USING (owner_id = auth.uid());

CREATE POLICY "Owners can view their own meters" 
  ON meters FOR SELECT 
  USING (property_id IN (SELECT id FROM properties WHERE owner_id = auth.uid()));

-- 2. Tenants see only their current assignment readings and ledger
CREATE POLICY "Tenants can view their current assignments"
  ON meter_assignments FOR SELECT
  USING (tenant_id = auth.uid());

CREATE POLICY "Tenants can view their meter readings"
  ON readings FOR SELECT
  USING (meter_id IN (SELECT meter_id FROM meter_assignments WHERE tenant_id = auth.uid() AND ended_at IS NULL));

-- ==========================================
-- DATA RETENTION JOBS (pg_cron)
-- ==========================================

-- Keep readings and device_diagnostics for a rolling 14 days
SELECT cron.schedule('daily-cleanup', '0 0 * * *', $$
  DELETE FROM readings WHERE recorded_at < now() - interval '14 days';
  DELETE FROM device_diagnostics WHERE reported_at < now() - interval '14 days';
$$);
