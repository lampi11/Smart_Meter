import { NextResponse } from 'next/server';
import { Pool } from 'pg';

const pool = new Pool({
  user: 'postgres',
  host: 'localhost',
  database: 'lampitex',
  password: 'mysecretpassword',
  port: 5432,
});

export async function POST(request: Request) {
  try {
    const data = await request.json();
    const { device_id, readings, balance_after, relay_on } = data;

    if (!device_id) {
      return NextResponse.json({ error: 'Invalid payload' }, { status: 400 });
    }

    const client = await pool.connect();
    
    const meterRes = await client.query('SELECT id, pending_message, pending_recharge FROM meters WHERE device_id = $1', [device_id]);
    
    let meterId;
    let pendingMessage = null;
    let pendingRecharge = 0;

    if (meterRes.rows.length === 0) {
      const orgRes = await client.query("SELECT id FROM organizations LIMIT 1");
      const propRes = await client.query("SELECT id FROM properties LIMIT 1");
      
      if (orgRes.rows.length > 0 && propRes.rows.length > 0) {
        const newMeter = await client.query(`
          INSERT INTO meters (device_id, organization_id, property_id, qr_code, label, balance, tariff_grid, relay_on)
          VALUES ($1, $2, $3, $4, $5, $6, $7, $8) RETURNING id
        `, [
          device_id, 
          orgRes.rows[0].id, 
          propRes.rows[0].id, 
          'LMPTX-' + device_id.substring(device_id.length - 5).replace(/:/g, ''), 
          'Meter ' + device_id.substring(device_id.length - 5),
          balance_after || 0,
          8.0,
          relay_on || true
        ]);
        meterId = newMeter.rows[0].id;
      } else {
        client.release();
        return NextResponse.json({ error: 'No org found to assign meter' }, { status: 404 });
      }
    } else {
      meterId = meterRes.rows[0].id;
      pendingMessage = meterRes.rows[0].pending_message;
      pendingRecharge = parseFloat(meterRes.rows[0].pending_recharge) || 0;
    }

    if (readings && Array.isArray(readings)) {
      for (const r of readings) {
        const date = new Date(r.timestamp * 1000);
        await client.query(`
          INSERT INTO readings (meter_id, recorded_at, voltage, current, power, energy_kwh, balance_after, relay_on)
          VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
        `, [meterId, date.toISOString(), r.voltage, r.current, r.power, r.energy, r.balance_after, r.relay_on]);
      }
    }

    // Update meter latest state and clear pending fields
    await client.query(`
      UPDATE meters 
      SET balance = COALESCE($1, balance), 
          relay_on = COALESCE($2, relay_on), 
          last_seen_at = NOW(),
          pending_message = NULL,
          pending_recharge = 0
      WHERE id = $3
    `, [balance_after, relay_on, meterId]);

    client.release();

    return NextResponse.json({ 
      success: true, 
      inserted: readings ? readings.length : 0,
      message: pendingMessage,
      recharge: pendingRecharge
    });
  } catch (error) {
    console.error('API /telemetry error:', error);
    return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
  }
}
