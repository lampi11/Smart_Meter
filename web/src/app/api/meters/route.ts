import { NextResponse } from 'next/server';
import { Pool } from 'pg';

const pool = new Pool({
  user: 'postgres',
  host: 'localhost',
  database: 'lampitex',
  password: 'mysecretpassword',
  port: 5432,
});

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const orgId = searchParams.get('orgId') || '11111111-1111-1111-1111-111111111111';

    const client = await pool.connect();
    
    // Fetch Organization
    const orgRes = await client.query('SELECT * FROM organizations WHERE id = $1', [orgId]);
    const organization = orgRes.rows[0];

    // Fetch Meters and Latest Reading
    const metersRes = await client.query(`
      SELECT 
        m.id, 
        m.device_id, 
        m.qr_code, 
        m.label, 
        m.balance, 
        m.relay_on,
        m.last_seen_at,
        (EXTRACT(EPOCH FROM (NOW() - m.last_seen_at)) < 15) as is_online,
        p.name as property_name,
        (SELECT json_build_object(
            'voltage', r.voltage, 
            'current', r.current, 
            'power', r.power,
            'energy_kwh', r.energy_kwh,
            'recorded_at', r.recorded_at
        ) FROM readings r WHERE r.meter_id = m.id ORDER BY r.recorded_at DESC LIMIT 1) as latest_reading
      FROM meters m
      JOIN properties p ON m.property_id = p.id
      WHERE m.organization_id = $1
    `, [orgId]);

    client.release();

    return NextResponse.json({
      organization,
      meters: metersRes.rows
    });
  } catch (error) {
    console.error('Database Error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
